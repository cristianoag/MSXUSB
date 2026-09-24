#include <algorithm>
#include <array>
#include <cassert>
#include <cstdarg>
#include <cstdint>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>
#include "flash.h"

enum class Kind { Empty, Amic, Amd040, Amd010, Sst, Unsupported };

struct Write {
    uint16_t cpu;
    uint32_t physical;
    uint8_t data;
};

struct Chip {
    Kind kind = Kind::Empty;
    std::vector<uint8_t> memory;
    std::array<uint8_t, 4> banks = {13, 9, 7, 3};
    std::vector<Write> writes;
    unsigned state = 0;
    unsigned busy = 0;
    unsigned programs = 0;
    unsigned erases = 0;
    unsigned invalid = 0;
    uint32_t pending_address = 0;
    uint8_t pending_value = 0xff;
    bool id = false;
    bool erase_pending = false;
    bool stuck = false;
    bool corrupt = false;

    explicit Chip(Kind type = Kind::Empty) : kind(type), memory(512 * 1024, 0x5a) {}

    bool sst() const { return kind == Kind::Sst || kind == Kind::Unsupported; }

    uint8_t manufacturer() const {
        if (sst()) return 0xbf;
        return kind == Kind::Amic ? 0x37 : 0x01;
    }

    uint8_t device() const {
        switch (kind) {
            case Kind::Amic: return 0x86;
            case Kind::Amd040: return 0xa4;
            case Kind::Amd010: return 0x20;
            case Kind::Sst: return 0xb7;
            default: return 0xb6;
        }
    }

    uint32_t physical(uint16_t cpu) const {
        auto address = uint32_t(banks[(cpu - 0x4000) / 0x2000]) * 0x2000 + (cpu & 0x1fff);
        return kind == Kind::Amd010 ? address & 0x1ffff : address;
    }

    void write(uint16_t cpu, uint8_t value) {
        const uint32_t address = physical(cpu);
        writes.push_back({cpu, address, value});
        // The CPLD latches the old bank for this bus cycle, then updates the register.
        if ((cpu & 0x1000) && (cpu & 0xff) == 0)
            banks[(cpu - 0x4000) / 0x2000] = value & 0x3f;
        if (kind == Kind::Empty || busy) return;
        if (state == 3) {
            pending_address = address;
            pending_value = value;
            busy = 8;
            ++programs;
            state = 0;
            return;
        }
        if (value == 0xf0) {
            id = false;
            state = 0;
            return;
        }
        if (id) return;

        const uint32_t command_address = address & (sst() ? 0x7fff : 0x7ff);
        const bool first = command_address == (sst() ? 0x5555 : 0x555);
        const bool second = command_address == (sst() ? 0x2aaa : 0x2aa);
        if (state == 0 && first && value == 0xaa) state = 1;
        else if (state == 1 && second && value == 0x55) state = 2;
        else if (state == 2 && first && value == 0x90) { id = true; state = 0; }
        else if (state == 2 && first && value == 0xa0) state = 3;
        else if (state == 2 && first && value == 0x80) state = 4;
        else if (state == 4 && first && value == 0xaa) state = 5;
        else if (state == 5 && second && value == 0x55) state = 6;
        else if (state == 6 && first && value == 0x10) {
            erase_pending = true;
            pending_value = 0xff;
            busy = 20;
            ++erases;
            state = 0;
        } else {
            if (state) ++invalid;
            state = 0;
        }
    }

    uint8_t read(uint16_t cpu) {
        if (kind == Kind::Empty) return 0xff;
        const uint32_t address = physical(cpu);
        if (busy) {
            if (!stuck && --busy == 0) {
                if (erase_pending) {
                    std::fill(memory.begin(), memory.end(), 0xff);
                    erase_pending = false;
                } else {
                    memory[pending_address] &= corrupt ? pending_value ^ 1 : pending_value;
                }
                // DQ7 can be ready before the other bits have settled.
                return pending_value ^ 1;
            }
            // SST DQ5 is deliberately high: it is not an AMD timeout indicator.
            return ((pending_value ^ 0x80) & 0x80) | ((busy & 1) ? 0x40 : 0) |
                ((sst() || stuck) ? 0x20 : 0);
        }
        if (id) {
            if (address == 0) return manufacturer();
            if (address == 1) return device();
            return 0xff;
        }
        return memory[address];
    }
};

static std::array<Chip, 4> slots;
static int page1 = -1;
static int page2 = -1;
static uint16_t stack_pointer = 0xf000;

void select_slot_40(uint8_t slot) { assert(slot < 4); page1 = slot; }
void select_slot_80(uint8_t slot) { assert(slot < 4); page2 = slot; }
void select_ramslot_40() { page1 = -1; }
void select_ramslot_80() { page2 = -1; }
static uint16_t ReadSP() { return stack_pointer; }

struct FlashBus {
    struct Byte {
        uint16_t cpu;
        Chip& chip() const {
            const int slot = cpu < 0x8000 ? page1 : page2;
            assert(slot >= 0 && slot < 4);
            return slots[slot];
        }
        operator uint8_t() const { return chip().read(cpu); }
        void operator=(uint8_t value) const { chip().write(cpu, value); }
    };
    Byte operator[](uint16_t offset) { return {uint16_t(0x4000 + offset)}; }
} flash_segment;

struct FileBuffer {
    std::array<uint8_t, 8192> data;
    uint8_t operator[](int offset) const {
        assert(page2 == -1); // Never read the file buffer while cartridge ROM hides it.
        return data.at(offset);
    }
    operator uint8_t*() {
        assert(page2 == -1);
        return data.data();
    }
} file_segment;

struct FCB {
    char name[8];
    char ext[3];
    unsigned long file_size;
};

static unsigned opens = 0;
static unsigned closes = 0;
static constexpr int FCB_SUCCESS = 0;
#define SEGMENT_SIZE (8 * 1024)

static int fcb_open(FCB* fcb) {
    assert(page1 == -1 && page2 == -1);
    ++opens;
    fcb->file_size = 1;
    return FCB_SUCCESS;
}

static void fcb_close(FCB*) {
    assert(page1 == -1 && page2 == -1);
    ++closes;
}

static int fcb_read(FCB*, uint8_t* buffer, unsigned) {
    assert(page1 == -1 && page2 == -1);
    buffer[0] = 0x42;
    return 1;
}

static void MemFill(uint8_t* buffer, uint8_t value, unsigned length) {
    assert(page1 == -1 && page2 == -1);
    memset(buffer, value, length);
}

#include "flash-under-test.inc"

static void reset() {
    for (auto& chip : slots) chip = Chip();
    page1 = page2 = -1;
    stack_pointer = 0xf000;
    opens = closes = 0;
    set_flash_type(FALSE);
}

static void check_restored() {
    assert(page1 == -1 && page2 == -1);
    for (const auto& chip : slots) assert(!chip.id);
}

static void test_chip(Kind kind) {
    reset();
    slots[1] = Chip(kind);
    assert(find_flash() == 1);
    assert(bool(flash_is_sst) == (kind == Kind::Sst));
    check_restored();
    slots[1].writes.clear();
    assert(erase_flash(1));
    check_restored();
    auto& chip = slots[1];
    assert(chip.erases == 1);
    assert(std::all_of(chip.memory.begin(), chip.memory.end(), [](uint8_t b) { return b == 0xff; }));
    const uint32_t mask = chip.sst() ? 0x7fff : 0x7ff;
    const uint32_t a1 = chip.sst() ? 0x5555 : 0x555;
    const uint32_t a2 = chip.sst() ? 0x2aaa : 0x2aa;
    // Erase must end with exactly six consecutive flash writes, without mapper writes.
    const uint8_t erase_data[] = {0xaa, 0x55, 0x80, 0xaa, 0x55, 0x10};
    for (unsigned i = 0; i < 6; ++i) {
        const auto& w = chip.writes[chip.writes.size() - 6 + i];
        assert(w.data == erase_data[i]);
        assert((w.physical & mask) == (i == 1 || i == 4 ? a2 : a1));
    }
    for (uint8_t bank : {0, 1, 2, 7, 8, 15}) {
        for (unsigned i = 0; i < file_segment.data.size(); ++i)
            file_segment.data[i] = uint8_t(i * 17 + bank);
        assert(write_flash_segment(1, bank));
        check_restored();
        assert(std::equal(file_segment.data.begin(), file_segment.data.end(),
            chip.memory.begin() + bank * 8192));
    }
    if (kind != Kind::Amd010) {
        assert(write_flash_segment(1, 63));
        assert(std::equal(file_segment.data.begin(), file_segment.data.end(),
            chip.memory.begin() + 63 * 8192));
    }
    assert(chip.invalid == 0);
    assert(chip.programs == (kind == Kind::Amd010 ? 6u : 7u) * 8192);
    check_restored();
}

static void test_detection() {
    reset();
    assert(find_flash() == 4);
    check_restored();
    slots[2] = Chip(Kind::Unsupported);
    assert(find_flash() == 4);
    check_restored();
    slots[0] = Chip(Kind::Sst);
    slots[2] = Chip(Kind::Amd040);
    assert(find_flash() == 2 && !flash_is_sst);
    check_restored();
    slots[2] = Chip(Kind::Sst);
    assert(find_flash() == 2 && flash_is_sst);
    check_restored();
    // Explicit-slot detection must also choose the chip's command set.
    assert(flash_ident(2) && flash_is_sst);
    check_restored();
    stack_pointer = 0xbfff;
    assert(!flash_ident(2));
    check_restored();
}

static void test_failures() {
    for (Kind kind : {Kind::Sst, Kind::Amd040}) {
        reset();
        slots[1] = Chip(kind);
        assert(flash_ident(1));
        slots[1].stuck = true;
        assert(!erase_flash(1));
        check_restored();

        reset();
        slots[1] = Chip(kind);
        assert(flash_ident(1));
        assert(erase_flash(1));
        file_segment.data.fill(0xa5);
        slots[1].corrupt = true;
        assert(!write_flash_segment(1, 2));
        assert(slots[1].programs == 1);
        check_restored();
    }
}

static void test_main() {
    char flag[] = "/S1";
    char filename[] = "TEST.ROM";
    char* arguments[] = {flag, filename};
    for (Kind kind : {Kind::Sst, Kind::Amd040, Kind::Unsupported}) {
        reset();
        slots[1] = Chip(kind);
        flash_main(arguments, 2);
        check_restored();
        if (kind == Kind::Unsupported) {
            assert(opens == 0 && slots[1].erases == 0);
        } else {
            assert(opens == 1 && closes == 1);
            assert(slots[1].erases == 1 && slots[1].programs == 8192);
            assert(slots[1].memory[0] == 0x42 && slots[1].memory[1] == 0xff);
        }
    }
    reset();
    slots[1] = Chip(Kind::Sst);
    flash_main(arguments, 1);
    assert(opens == 0 && slots[1].erases == 0);
    check_restored();
    slots[1].stuck = true;
    flash_main(arguments + 1, 1);
    assert(opens == 1 && closes == 1);
    assert(slots[1].erases == 1 && slots[1].programs == 0);
    check_restored();
}

int main() {
    for (Kind kind : {Kind::Amic, Kind::Amd040, Kind::Amd010, Kind::Sst}) test_chip(kind);
    test_detection();
    test_failures();
    test_main();
    puts("PASS: all four chips, banked command sequences, programming, detection and failure cleanup");
}
