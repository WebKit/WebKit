import * as memory from "./memory.wasm"
import * as assert from '../assert.js';

assert.instanceof(memory.memory, WebAssembly.Memory);
let buffer = new Uint8Array(memory.memory.buffer);
assert.eq(buffer[4], 0x10);
assert.eq(buffer[5], 0x00);
assert.eq(buffer[6], 0x10);
assert.eq(buffer[7], 0x00);
assert.throws(() => {
    memory.memory = 200;
}, TypeError, `Attempted to assign to readonly property.`);
