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

function assertIsolatedResizableBufferOfPageSize(pageCount, buffer, maxPageCount) {
    assertTrue(buffer instanceof ArrayBuffer);
    assertTrue(buffer.resizable);
    assertEq(buffer.byteLength, pageCount * pageSize);
    assertEq(buffer.maxByteLength, maxPageCount * pageSize);
}

function assertDetachedBuffer(buffer) {
    assertTrue(buffer instanceof ArrayBuffer);
    assertTrue(buffer.detached);
    assertEq(buffer.byteLength, 0);
    assertEq(buffer.maxByteLength, 0);
}

function assertSharedGrowableBufferOfPageSize(pageCount, buffer, maxPageCount) {
    assertTrue(buffer instanceof SharedArrayBuffer);
    assertTrue(buffer.growable);
    assertEq(buffer.byteLength, pageCount * pageSize);
    assertEq(buffer.maxByteLength, maxPageCount * pageSize);
}

{
    let memory = new WebAssembly.Memory({initial: 1, maximum: 5});
    let buffer = memory.toResizableBuffer();
    assertIsolatedResizableBufferOfPageSize(1, buffer, 5);
    assertEq(buffer, memory.buffer);

    // Growing the memory, the buffer should stay associated and properly refreshed
    memory.grow(1);
    assertEq(buffer, memory.buffer);
    assertIsolatedResizableBufferOfPageSize(2, buffer, 5);

    // Growing the memory via the buffer .resize API
    buffer.resize(3 * pageSize);
    assertEq(buffer, memory.buffer);
    assertIsolatedResizableBufferOfPageSize(3, buffer, 5);

    // Growing the memory via the buffer .resize API - no growth
    buffer.resize(3 * pageSize);
    assertEq(buffer, memory.buffer);
    assertIsolatedResizableBufferOfPageSize(3, buffer, 5);

    // Growing the memory via the buffer .resize API - illegal args: shrinking
    assertThrows(() => buffer.resize(2 * pageSize), RangeError, "");

    // Growing the memory via the buffer .resize API - illegal args: size not a page multiple
    assertThrows(() => buffer.resize(4 * pageSize + 1), RangeError, "");

    // Asking for a fixed length buffer should detach the old resizable one
    let buffer2 = memory.toFixedLengthBuffer();
    assertEq(buffer2, memory.buffer);
    assertDetachedBuffer(buffer);
    assertTrue(memory.buffer !== buffer);
}

// Now a similar sequence for a shared memory

{
    let memory = new WebAssembly.Memory({initial: 1, maximum: 5, shared: true});
    let buffer = memory.toResizableBuffer();
    assertSharedGrowableBufferOfPageSize(1, buffer, 5);
    assertEq(buffer, memory.buffer);

    // Growing the memory, the buffer should stay associated and properly refreshed
    memory.grow(1);
    assertEq(buffer, memory.buffer);
    assertSharedGrowableBufferOfPageSize(2, buffer, 5);

    // Growing the memory via the buffer .grow API
    buffer.grow(3 * pageSize);
    assertEq(buffer, memory.buffer);
    assertSharedGrowableBufferOfPageSize(3, buffer, 5);

    // Growing the memory via the buffer .grow API - no growth
    buffer.grow(3 * pageSize);
    assertEq(buffer, memory.buffer);
    assertSharedGrowableBufferOfPageSize(3, buffer, 5);

    // Growing the memory via the buffer .grow API - illegal args: shrinking
    assertThrows(() => buffer.grow(2 * pageSize), RangeError, "");

    // Growing the memory via the buffer .grow API - illegal args: size not a page multiple
    assertThrows(() => buffer.grow(4 * pageSize + 1), RangeError, "");

    // Asking for a fixed length buffer should disassociate the current one
    let buffer2 = memory.toFixedLengthBuffer();
    assertTrue(memory.buffer !== buffer);
    assertTrue(memory.buffer === buffer2);
    // The old buffer is not the associated buffer anymore but should track memory length
    memory.grow(1);
    assertEq(memory.buffer.byteLength, 4 * pageSize);
    assertSharedGrowableBufferOfPageSize(4, buffer, 5);
}

// Non-shared memory with no user-specified maximum

{
    let memory = new WebAssembly.Memory({ initial: 0 });
    let buffer = memory.toResizableBuffer();
    const expectedPageCount = is32BitPlatform() ? 32767 : 65536;
    assertEq(buffer.maxByteLength, pageSize * expectedPageCount);
}