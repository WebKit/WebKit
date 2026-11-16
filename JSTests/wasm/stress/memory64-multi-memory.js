//@ requireOptions("--useWasmMultiMemory=1", "--useWasmMemory64=1")
//@ skip if $addressBits <= 32

import * as assert from "../assert.js";
import { instantiate } from "../wabt-wrapper.js";

// A module may mix memory32 and memory64. The address width of an access follows the memory it
// targets, so a module whose memory 0 is 64-bit must still narrow i32 addresses for its memory32s,
// and vice versa.

async function test(wat, fn, imports = {}, options = {}) {
    const instance = await instantiate(wat, imports, { multi_memory: true, memory64: true, ...options });
    fn(instance.exports);
}

// Memory 0 is 64-bit, memory 1 is 32-bit.
await test(`
(module
  (memory i64 1)
  (memory 1)
  (func (export "store64") (param i64 i32) (local.get 0) (local.get 1) (i32.store 0))
  (func (export "load64") (param i64) (result i32) (local.get 0) (i32.load 0))
  (func (export "store32") (param i32 i32) (local.get 0) (local.get 1) (i32.store 1))
  (func (export "load32") (param i32) (result i32) (local.get 0) (i32.load 1))
)`, (e) => {
    e.store64(8n, 0xcafe);
    e.store32(8, 0xbeef);
    assert.eq(e.load64(8n), 0xcafe);
    assert.eq(e.load32(8), 0xbeef);
});

// Memory 0 is 32-bit, memory 1 is 64-bit. A memory64 address must keep its full width even when
// memory 0 is 32-bit, rather than being truncated to the width of memory 0.
await test(`
(module
  (memory 1)
  (memory i64 1)
  (func (export "store32") (param i32 i32) (local.get 0) (local.get 1) (i32.store 0))
  (func (export "load32") (param i32) (result i32) (local.get 0) (i32.load 0))
  (func (export "store64") (param i64 i32) (local.get 0) (local.get 1) (i32.store 1))
  (func (export "load64") (param i64) (result i32) (local.get 0) (i32.load 1))
)`, (e) => {
    e.store32(16, 0x1234);
    e.store64(16n, 0x5678);
    assert.eq(e.load32(16), 0x1234);
    assert.eq(e.load64(16n), 0x5678);
});

// An i32 address on the IPInt stack may hold garbage in its upper half, because i32.wrap_i64 is a
// no-op there. Accessing a memory32 in a module whose memory 0 is memory64 must still ignore those
// bits rather than folding them into the address.
await test(`
(module
  (memory i64 1)
  (memory 1)
  (func (export "storeWrapped") (param i64 i32)
    (i32.wrap_i64 (local.get 0)) (local.get 1) (i32.store 1))
  (func (export "loadWrapped") (param i64) (result i32)
    (i32.wrap_i64 (local.get 0)) (i32.load 1))
  ;; Force the multi-memory slow path with a large offset as well.
  (func (export "storeWrappedOffset") (param i64 i32)
    (i32.wrap_i64 (local.get 0)) (local.get 1) (i32.store 1 offset=256))
  (func (export "loadWrappedOffset") (param i64) (result i32)
    (i32.wrap_i64 (local.get 0)) (i32.load 1 offset=256))
)`, (e) => {
    for (const garbage of [0n, 1n, 0xffffffffn, 0x123456789n]) {
        const addr = (garbage << 32n) | 32n;
        e.storeWrapped(addr, 0xaaaa);
        assert.eq(e.loadWrapped(addr), 0xaaaa);
        e.storeWrappedOffset(addr, 0xbbbb);
        assert.eq(e.loadWrappedOffset(addr), 0xbbbb);
    }
    // The two accesses target distinct addresses, so neither clobbered the other.
    assert.eq(e.loadWrapped((0x123456789n << 32n) | 32n), 0xaaaa);
});

// Bulk memory across mixed widths.
await test(`
(module
  (memory i64 1)
  (memory 1)
  (data "wxyz")
  (func (export "init64") (i64.const 0) (i32.const 0) (i32.const 4) (memory.init 0 0))
  (func (export "init32") (i32.const 0) (i32.const 0) (i32.const 4) (memory.init 1 0))
  (func (export "copyTo32") (i32.const 64) (i64.const 0) (i32.const 4) (memory.copy 1 0))
  (func (export "copyTo64") (i64.const 64) (i32.const 0) (i32.const 4) (memory.copy 0 1))
  (func (export "fill64") (i64.const 128) (i32.const 7) (i64.const 4) (memory.fill 0))
  (func (export "fill32") (i32.const 128) (i32.const 9) (i32.const 4) (memory.fill 1))
  (func (export "load64") (param i64) (result i32) (local.get 0) (i32.load 0))
  (func (export "load32") (param i32) (result i32) (local.get 0) (i32.load 1))
)`, (e) => {
    e.init64();
    e.init32();
    const wxyz = 0x7a797877;
    assert.eq(e.load64(0n), wxyz);
    assert.eq(e.load32(0), wxyz);

    e.copyTo32();
    assert.eq(e.load32(64), wxyz);
    e.copyTo64();
    assert.eq(e.load64(64n), wxyz);

    e.fill64();
    e.fill32();
    assert.eq(e.load64(128n), 0x07070707);
    assert.eq(e.load32(128), 0x09090909);
});

// memory.size and memory.grow keep their per-memory result types.
await test(`
(module
  (memory i64 1)
  (memory 1)
  (func (export "size64") (result i64) (memory.size 0))
  (func (export "size32") (result i32) (memory.size 1))
  (func (export "grow64") (param i64) (result i64) (local.get 0) (memory.grow 0))
  (func (export "grow32") (param i32) (result i32) (local.get 0) (memory.grow 1))
)`, (e) => {
    assert.eq(e.size64(), 1n);
    assert.eq(e.size32(), 1);
    assert.eq(e.grow64(1n), 1n);
    assert.eq(e.grow32(2), 1);
    assert.eq(e.size64(), 2n);
    assert.eq(e.size32(), 3);
});

// Out-of-bounds still traps on the correct memory, and a memory64 address above 4GiB is not
// silently truncated into a valid memory32 offset.
await test(`
(module
  (memory 1)
  (memory i64 1)
  (func (export "load32") (param i32) (result i32) (local.get 0) (i32.load 0))
  (func (export "load64") (param i64) (result i32) (local.get 0) (i32.load 1))
)`, (e) => {
    assert.throws(() => e.load32(0x10000), WebAssembly.RuntimeError, "Out of bounds memory access");
    assert.throws(() => e.load64(0x10000n), WebAssembly.RuntimeError, "Out of bounds memory access");
    // 4GiB exactly: truncating to 32 bits would make this address 0, which is in bounds.
    assert.throws(() => e.load64(0x100000000n), WebAssembly.RuntimeError, "Out of bounds memory access");
});

// Imported memories of mixed widths, and more than two memories.
await test(`
(module
  (import "m" "a" (memory i64 1))
  (import "m" "b" (memory 1))
  (memory i64 1)
  (memory 1)
  (func (export "store") (param i64 i32)
    (local.get 0) (local.get 1) (i32.store 0)
    (i32.wrap_i64 (local.get 0)) (local.get 1) (i32.store 1)
    (local.get 0) (local.get 1) (i32.store 2)
    (i32.wrap_i64 (local.get 0)) (local.get 1) (i32.store 3))
  (func (export "check") (param i64) (result i32)
    (i32.and
      (i32.and
        (i32.load 0 (local.get 0))
        (i32.load 1 (i32.wrap_i64 (local.get 0))))
      (i32.and
        (i32.load 2 (local.get 0))
        (i32.load 3 (i32.wrap_i64 (local.get 0))))))
)`, (e) => {
    e.store(24n, 0x3333);
    assert.eq(e.check(24n), 0x3333);
}, {
    m: {
        a: new WebAssembly.Memory({ initial: 1n, address: "i64" }),
        b: new WebAssembly.Memory({ initial: 1 }),
    },
});

// Atomics resolve the address width per memory too, including the wait/notify paths.
await test(`
(module
  (memory i64 1 1 shared)
  (memory 1 1 shared)
  (func (export "add64") (param i64 i32) (result i32) (local.get 0) (local.get 1) (i32.atomic.rmw.add 0))
  (func (export "add32") (param i32 i32) (result i32) (local.get 0) (local.get 1) (i32.atomic.rmw.add 1))
  (func (export "load64") (param i64) (result i32) (local.get 0) (i32.atomic.load 0))
  (func (export "load32") (param i32) (result i32) (local.get 0) (i32.atomic.load 1))
  (func (export "notify64") (param i64) (result i32) (local.get 0) (i32.const 1) (memory.atomic.notify 0))
  (func (export "wait64") (param i64) (result i32)
    (local.get 0) (i32.const 999) (i64.const 0) (memory.atomic.wait32 0))
  (func (export "wait32") (param i32) (result i32)
    (local.get 0) (i32.const 999) (i64.const 0) (memory.atomic.wait32 1))
)`, (e) => {
    e.add64(8n, 5);
    e.add32(8, 7);
    assert.eq(e.load64(8n), 5);
    assert.eq(e.load32(8), 7);
    e.notify64(8n);
    // The stored value does not match, so the wait reports "not-equal" rather than blocking. A
    // wrong address width would read a different memory and change that answer.
    assert.eq(e.wait64(8n), 1);
    assert.eq(e.wait32(8), 1);
    assert.throws(() => e.wait64(0x100000000n), WebAssembly.RuntimeError, "Out of bounds memory access");
}, {}, { threads: true });

// Memories past index 63 fall in the second word of the address-width bitmap.
{
    let memories = "";
    for (let i = 0; i < 70; ++i)
        memories += (i % 2) ? "(memory i64 1)" : "(memory 1)";
    await test(`
(module
  ${memories}
  (func (export "store64") (param i64 i32) (local.get 0) (local.get 1) (i32.store 65))
  (func (export "load64") (param i64) (result i32) (local.get 0) (i32.load 65))
  (func (export "store32") (param i32 i32) (local.get 0) (local.get 1) (i32.store 66))
  (func (export "load32") (param i32) (result i32) (local.get 0) (i32.load 66))
)`, (e) => {
        e.store64(8n, 0x1111);
        e.store32(8, 0x2222);
        assert.eq(e.load64(8n), 0x1111);
        assert.eq(e.load32(8), 0x2222);
        assert.throws(() => e.load64(0x100000000n), WebAssembly.RuntimeError, "Out of bounds memory access");
    });
}
