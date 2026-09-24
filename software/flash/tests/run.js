const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const { spawnSync } = require('node:child_process');

// Compile the actual protocol code against a host-side flash/mapper bus model.
// Only SDCC absolute storage, unused debug helpers and BIOS assembly are excluded.
const sourceDir = path.resolve(__dirname, '..', 'src');
const source = fs.readFileSync(path.join(sourceDir, 'flash.c'), 'utf8').replace(/\r\n/g, '\n');
const globals = source.match(/static BOOL flash_is_sst[^;]*;[\s\S]*?static uint16_t flash_address2[^;]*;/);
const coreStart = source.indexOf('static void set_flash_type (');
const mainStart = source.indexOf('void FT_SetName(');
const mainEnd = source.indexOf('/*\n    ; select slot 40');
const mainSignature = 'int main(char *argv[], int argc)';
if (!globals || coreStart < 0 || mainStart < 0 || mainEnd < mainStart || !source.includes(mainSignature)) {
    throw new Error('Cannot locate flash protocol code for the host test');
}

const temporary = fs.mkdtempSync(path.join(os.tmpdir(), 'msxusb-flash-test-'));
try {
    fs.writeFileSync(path.join(temporary, 'flash-under-test.inc'),
        globals[0] + '\n' + source.slice(coreStart) + '\n' +
        source.slice(mainStart, mainEnd).replace(mainSignature, 'int flash_main(char *argv[], int argc)'));
    const executable = path.join(temporary, process.platform === 'win32' ? 'flash-test.exe' : 'flash-test');
    for (const [command, args] of [
        [process.env.CXX || 'g++', ['-std=c++17', '-Wall', '-Wextra', '-Werror', '-Wno-char-subscripts',
            '-I', sourceDir, '-I', temporary, path.join(__dirname, 'flash_test.cpp'), '-o', executable]],
        [executable, []],
    ]) {
        const result = spawnSync(command, args, { stdio: 'inherit' });
        if (result.error) throw result.error;
        if (result.status !== 0) throw new Error(`${command} failed: ${result.status}`);
    }
} finally {
    fs.rmSync(temporary, { recursive: true });
}
