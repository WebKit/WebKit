//@ $skipModes << "wasm-no-jit".to_sym if $buildType == "debug"

import { eq as assertEq, throws as assertThrows } from "../assert.js";
const pageSize = 64*1024;

function assertTrue(expr) {
    assertEq(expr, true);
}

let buffers = [];
for (let i = 0; i < 100; i++) {
    const max = 5;
    let pageCount = 1;
    let x = new WebAssembly.Memory({initial: 1, maximum: max});
    for (let i = 0; i < (max - 1); i++) {
        let int8Array = new Uint8Array(x.buffer);

        for (let i = 0; i < pageSize; i++) {
            assertEq(int8Array[pageSize*(pageCount - 1) + i], 0);
            int8Array[pageSize*(pageCount - 1) + i] = pageCount;
        }

        for (let i = 0; i < pageCount; i++) {
            for (let j = 0; j < pageSize; j++) {
                assertEq(int8Array[i * pageSize + j], i + 1);
            }
        }

        let buffer = x.buffer;
        assertEq(buffer.byteLength, pageCount * pageSize);
        buffers.push(buffer);
        let previousPageSize = x.grow(1);
        assertEq(buffer.byteLength, 0);
        assertEq(previousPageSize, pageCount);
        ++pageCount;
    }
}

for (let buffer of buffers) {
    assertEq(buffer.byteLength, 0);
}

{
    const memory = new WebAssembly.Memory({initial: 1, maximum: 5});
    let buffer = memory.buffer;
    assertTrue(buffer instanceof ArrayBuffer);
    assertEq(buffer.byteLength, 1*64*1024);
    memory.grow(1);
    assertTrue(buffer.detached);
    assertEq(buffer.byteLength, 0);

    buffer = memory.buffer;
    assertEq(buffer.byteLength, 2*64*1024);

    // This shouldn't neuter the buffer since it fails.
    assertThrows(() => memory.grow(1000), RangeError, "WebAssembly.Memory.grow would exceed the memory's declared maximum size");
    assertEq(buffer.byteLength, 2*64*1024);
    assertEq(memory.buffer, buffer);
}

// Now the same for a shared memory/buffer
{
    const memory = new WebAssembly.Memory({initial: 1, maximum: 5, shared: true});
    let buffer = memory.buffer;
    assertTrue(buffer instanceof SharedArrayBuffer);
    assertEq(buffer.byteLength, 1*64*1024);
    memory.grow(1);
    assertEq(buffer.byteLength, 1*64*1024);

    let buffer2 = memory.buffer;
    assertTrue(buffer !== buffer2);
    assertEq(buffer2.byteLength, 2*64*1024);

    // This shouldn't alter the buffer since it fails.
    assertThrows(() => memory.grow(1000), RangeError, "WebAssembly.Memory.grow would exceed the memory's declared maximum size");
    assertEq(buffer2.byteLength, 2*64*1024);
    assertEq(memory.buffer, buffer2);
}
