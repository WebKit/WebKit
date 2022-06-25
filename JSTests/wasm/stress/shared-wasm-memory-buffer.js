//Growing a shared memory requires signal handlers, which are not yet ported to ARMv7
//@ skip if $architecture == "arm"
import * as assert from '../assert.js'

let pageSize = 64 << 10;
let memory = new WebAssembly.Memory({
    initial: 1,
    maximum: 5,
    shared: true,
});
let buffer1 = memory.buffer;
assert.eq(buffer1.byteLength, pageSize * 1);
assert.eq(buffer1, memory.buffer);

memory.grow(1);
let buffer2 = memory.buffer;
assert.eq(buffer1.byteLength, pageSize * 1);
assert.eq(buffer2.byteLength, pageSize * 2);
assert.falsy(buffer1 === buffer2);

let bytes1 = new Uint8Array(buffer1);
let bytes2 = new Uint8Array(buffer2);
bytes1[0] = 42;
assert.eq(bytes2[0], 42);
bytes2[0] = 43;
assert.eq(bytes1[0], 43);
