# MSXUSB external Nextor driver firmware
#
# This builds a driver-first 128KB ROM image. It is meant for use when another
# cartridge boots Nextor and should discover MSXUSB as an external Nextor
# driver.

ASM = ../../MsxUsbNext/msx/sjasmplus.exe
ASMFLAGS = --fullpath --raw=../../NextorUSBHost_ext/dist/nextorusbhost_ext_driver.rom --sym=../../NextorUSBHost_ext/dist/nextorusbhost_ext.sym --lst=../../NextorUSBHost_ext/dist/nextorusbhost_ext.lst -D__MSXUSBCARTv1

OUTPUT_DIR = ../dist
ROM = $(OUTPUT_DIR)/nextorusbhost_ext.rom
DRIVER_ROM = $(OUTPUT_DIR)/nextorusbhost_ext_driver.rom
ROM_SIZE = 131072

SOURCES = \
    driver_low.asm \
    driver_helpers_low.asm \
    flashdisk.asm \
    print_bios.asm \
    ch376s.asm \
    ch376s_helpers.asm \
    usbhost.asm \
    nextordirect.asm \
    scsi.asm \
    scsi_helpers.asm

all: $(ROM)

.PHONY: all clean

$(OUTPUT_DIR):
	mkdir -p $(OUTPUT_DIR)

$(DRIVER_ROM): $(OUTPUT_DIR) $(SOURCES)
	$(ASM) $(ASMFLAGS) driver_low.asm

$(ROM): $(DRIVER_ROM)
	dd if=/dev/zero of=$(ROM) bs=$(ROM_SIZE) count=1
	dd if=$(DRIVER_ROM) of=$(ROM) conv=notrunc

clean:
	rm -f $(OUTPUT_DIR)/*.rom
	rm -f $(OUTPUT_DIR)/*.sym
	rm -f $(OUTPUT_DIR)/*.lst
