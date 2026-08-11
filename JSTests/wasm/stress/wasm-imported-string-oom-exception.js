//@ requireOptions("--useConcurrentJIT=0")
//@ skip if $memoryLimited or $addressBits <= 32

function main() {
    const wasmCode = new Uint8Array([0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00]);

    const options = {
        importedStringConstants: 'a'.repeat(0x7ffffffe) + '\u1234',
        builtins: []
    };

    new WebAssembly.Module(wasmCode, options);
}

try { main(); } catch(e) { }
