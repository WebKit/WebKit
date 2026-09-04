//@ skip if $addressBits <= 32
import { instantiate } from "../wabt-wrapper.js";
import * as assert from "../assert.js";

const oob = "Out of bounds memory access";
const page = 65536;

const wat = `
(module
  (memory (export "memory") 1)
  (func (export "fill") (param i32 i32 i32)
    (memory.fill (local.get 0) (local.get 1) (local.get 2)))
  (func (export "fillAdd") (param i32 i32 i32 i32)
    (memory.fill (i32.add (local.get 0) (local.get 1)) (local.get 2) (local.get 3)))
  (func (export "copy") (param i32 i32 i32)
    (memory.copy (local.get 0) (local.get 1) (local.get 2)))
  (func (export "copyAdd") (param i32 i32 i32 i32)
    (memory.copy (i32.add (local.get 0) (local.get 1)) (local.get 2) (local.get 3)))
  (func (export "grow") (param i32) (result i32)
    (memory.grow (local.get 0)))
)
`;

const wat64 = `
(module
  (memory (export "memory") i64 1)
  (func (export "fill") (param i64 i32 i64)
    (memory.fill (local.get 0) (local.get 1) (local.get 2)))
  (func (export "copy") (param i64 i64 i64)
    (memory.copy (local.get 0) (local.get 1) (local.get 2)))
)
`;

function writePattern(memory, start, length) {
    const view = new Uint8Array(memory.buffer, start, length);
    for (let i = 0; i < length; i++)
        view[i] = i + 1;
}

function assertBytes(memory, start, expected) {
    const view = new Uint8Array(memory.buffer, start, expected.length);
    for (let i = 0; i < expected.length; i++)
        assert.eq(view[i], expected[i]);
}

function assertThrowsOOB(fn) {
    assert.throws(fn, WebAssembly.RuntimeError, oob);
}

async function testMemory32() {
    const { fill, fillAdd, copy, copyAdd, grow, memory } = (await instantiate(wat, {}, { bulk_memory: true })).exports;

    fill(0, 0, 16);
    fill(4, 0x1ab, 4);
    assertBytes(memory, 0, [0, 0, 0, 0, 0xab, 0xab, 0xab, 0xab, 0]);

    fill(0, 0, 16);
    fillAdd(2, 2, 0xcd, 2);
    assertBytes(memory, 2, [0, 0, 0xcd, 0xcd, 0]);

    writePattern(memory, 0, 8);
    copy(4, 0, 8);
    assertBytes(memory, 0, [1, 2, 3, 4, 1, 2, 3, 4, 5, 6, 7, 8]);

    writePattern(memory, 0, 8);
    copy(2, 0, 6);
    assertBytes(memory, 0, [1, 2, 1, 2, 3, 4, 5, 6]);

    writePattern(memory, 0, 8);
    copy(0, 2, 6);
    assertBytes(memory, 0, [3, 4, 5, 6, 7, 8, 7, 8]);

    writePattern(memory, 0, 8);
    copy(0, 0, 8);
    assertBytes(memory, 0, [1, 2, 3, 4, 5, 6, 7, 8]);

    writePattern(memory, 0, 8);
    copyAdd(2, 2, 0, 4);
    assertBytes(memory, 0, [1, 2, 3, 4, 1, 2, 3, 4]);

    fill(0, 0xaa, 8);
    assertThrowsOOB(() => fill(4, 0xbb, page));
    assertBytes(memory, 0, [0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa]);

    writePattern(memory, 0, 8);
    assertThrowsOOB(() => copy(4, 0, page));
    assertBytes(memory, 0, [1, 2, 3, 4, 5, 6, 7, 8]);

    fill(page, 0, 0);
    copy(page, page, 0);
    assertThrowsOOB(() => fill(page + 1, 0, 0));
    assertThrowsOOB(() => copy(page + 1, 0, 0));
    assertThrowsOOB(() => copy(0, page + 1, 0));

    assertThrowsOOB(() => fill(-1, 0, 1));
    assertThrowsOOB(() => fill(page - 1, 0, 2));
    assertThrowsOOB(() => copy(-1, 0, 1));
    assertThrowsOOB(() => copy(0, -1, 1));
    assertThrowsOOB(() => copy(0xfffffff0, 0, 32));

    for (let i = 0; i < wasmTestLoopCount; i++) {
        fill(0, i & 0xff, 32);
        copy(32, 0, 32);
        assertBytes(memory, 0, [i & 0xff, i & 0xff]);
        assertBytes(memory, 31, [i & 0xff, i & 0xff]);
        assertBytes(memory, 63, [i & 0xff]);
    }

    assert.eq(grow(1), 1);
    fill(page, 0xcd, 4);
    assertBytes(memory, page, [0xcd, 0xcd, 0xcd, 0xcd]);
    fill(page * 2, 0, 0);
    copy(page * 2, page * 2, 0);
    assertThrowsOOB(() => fill(page * 2, 0, 1));
    assertThrowsOOB(() => copy(page * 2, 0, 1));
    assertThrowsOOB(() => copy(0, page * 2, 1));
}

async function testMemory64() {
    const { fill, copy, memory } = (await instantiate(wat64, {}, { bulk_memory: true, memory64: true })).exports;

    fill(0n, 0x7f, 4n);
    assertBytes(memory, 0, [0x7f, 0x7f, 0x7f, 0x7f]);
    copy(4n, 0n, 4n);
    assertBytes(memory, 4, [0x7f, 0x7f, 0x7f, 0x7f]);

    fill(BigInt(page), 0, 0n);
    copy(BigInt(page), BigInt(page), 0n);
    assertThrowsOOB(() => fill(BigInt(page) + 1n, 0, 0n));
    assertThrowsOOB(() => copy(BigInt(page) + 1n, 0n, 0n));

    assertThrowsOOB(() => fill(-1n, 0, 1n));
    assertThrowsOOB(() => fill(1n, 0, -1n));
    assertThrowsOOB(() => copy(-1n, 0n, 1n));
    assertThrowsOOB(() => copy(0n, -1n, 1n));
    assertThrowsOOB(() => copy(0n, 1n, -1n));
}

await assert.asyncTest((async () => {
    await testMemory32();
    await testMemory64();
})());
