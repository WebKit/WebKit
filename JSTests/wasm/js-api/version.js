import * as assert from '../assert.js';
import * as utilities from '../utilities.js';

for (let version = 0; version < 256; ++version) {
    if (version === 1)
        continue;
    const emptyModuleArray = Uint8Array.of(0x0, 0x61, 0x73, 0x6d, version, 0x00, 0x00, 0x00);
    assert.compileError(emptyModuleArray, `WebAssembly.Module doesn't parse at byte 0: unexpected version number ${version} expected 1`);
}
