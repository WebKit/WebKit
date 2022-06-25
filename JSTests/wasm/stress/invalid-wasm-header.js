import * as assert from '../assert.js';

let bytes = new Uint8Array([
  0,97,115,108,1,0,0,0,1,8,1,96,1,124,3,127,127,127,3,2,1,0,
]);
assert.throws(() => {
    let module = new WebAssembly.Module(bytes);
}, WebAssembly.CompileError, `WebAssembly.Module doesn't parse at byte 0: module doesn't start with '\\0asm' (evaluating 'new WebAssembly.Module(bytes)')`);
