//@ requireOptions("--useWasmMultiMemory=1")

import * as assert from "../assert.js";

const bytearray = new Uint8Array([
    0x00, 0x61, 0x73, 0x6d, // "\0asm"
    0x01, 0x00, 0x00, 0x00,
    0x05, 0x03, 0x01, 0x00, 0x0a, // memory section, length 3
    0x05, 0x03, 0x01, 0x00, 0x0a, // duplicate memory section
]);

WebAssembly.instantiate(bytearray).then($vm.abort, function (error) {
    assert.eq(String(error), `CompileError: WebAssembly.Module doesn't parse at byte 13: invalid section order, Memory followed by Memory`);
}).then(function() {}, $vm.abort);
