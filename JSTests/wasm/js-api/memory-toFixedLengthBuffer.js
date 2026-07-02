//@ requireOptions("--useWasmMemoryToBufferAPIs=true")

import { eq as assertEq, throws as assertThrows } from "../assert.js";
const KB = 1024;
const pageSize = 64*KB;

function assertTrue(expr) {
    assertEq(expr, true);
}

function assertFalse(expr) {
    assertEq(expr, false);
}

function assertIsolatedFixedLengthBufferOfPageSize(pageCount, buffer) {
    assertTrue(buffer instanceof ArrayBuffer);
    assertFalse(buffer.resizable);
    assertEq(buffer.byteLength, pageCount * pageSize);
    assertEq(buffer.maxByteLength, pageCount * pageSize);
}

function assertDetachedBuffer(buffer) {
    assertTrue(buffer instanceof ArrayBuffer);
    assertTrue(buffer.detached);
    assertEq(buffer.byteLength, 0);
    assertEq(buffer.maxByteLength, 0);
}

function assertSharedFixedLengthBufferOfPageSize(pageCount, buffer) {
    assertTrue(buffer instanceof SharedArrayBuffer);
    assertEq(buffer.byteLength, pageCount * pageSize);
    assertEq(buffer.maxByteLength, pageCount * pageSize);
}

{
    let memory = new WebAssembly.Memory({initial: 1, maximum: 5});
    let buffer = memory.toFixedLengthBuffer();
    assertEq(buffer, memory.buffer);
    assertIsolatedFixedLengthBufferOfPageSize(1, buffer);

    // Growing the memory should detach this buffer
    memory.grow(1);
    assertDetachedBuffer(buffer);

    // The buffer returned now should reflect the grown memory
    let buffer2 = memory.toFixedLengthBuffer();
    assertEq(buffer2, memory.buffer);
    assertIsolatedFixedLengthBufferOfPageSize(2, buffer2);

    // If we first get .buffer, .toFixedLengthBuffer() we get later is the same
    memory.grow(1);
    assertDetachedBuffer(buffer2);
    let buffer3 = memory.buffer;
    assertEq(memory.toFixedLengthBuffer(), buffer3);
    assertIsolatedFixedLengthBufferOfPageSize(3, buffer3);

    // If we get a resizable buffer, the old fixed one is detached
    let buffer4 = memory.toResizableBuffer();
    assertEq(buffer4, memory.buffer);
    assertDetachedBuffer(buffer3);
}

// Now the same but with a shared memory

{
    let memory = new WebAssembly.Memory({initial: 1, maximum: 5, shared: true});
    let buffer = memory.toFixedLengthBuffer();
    assertEq(buffer, memory.buffer);
    assertSharedFixedLengthBufferOfPageSize(1, buffer);

    // Growing the memory does not detach the old buffer but it's no longer the memory's buffer and should reflect the old length
    memory.grow(1);
    assertTrue(memory.buffer !== buffer);
    assertSharedFixedLengthBufferOfPageSize(1, buffer);

    // The buffer returned now should reflect the grown memory
    let buffer2 = memory.toFixedLengthBuffer();
    assertEq(buffer2, memory.buffer);
    assertSharedFixedLengthBufferOfPageSize(2, buffer2);

    // If we first get .buffer, .toFixedLengthBuffer() we get later is the same buffer
    memory.grow(1);
    let buffer3 = memory.buffer;
    assertEq(memory.toFixedLengthBuffer(), buffer3);
    assertSharedFixedLengthBufferOfPageSize(3, buffer3);
    assertSharedFixedLengthBufferOfPageSize(2, buffer2);

    // If we get a resizable buffer, the old fixed one should stop tracking memory size
    let buffer4 = memory.toResizableBuffer();
    assertEq(buffer4, memory.buffer);
    assertTrue(buffer3 !== memory.buffer);
    memory.grow(1);
    assertEq(buffer4, memory.buffer);
    assertSharedFixedLengthBufferOfPageSize(3, buffer3);
}
