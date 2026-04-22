//@ skip if $addressBits <= 32
//@ runDefaultWasm("-m", "--useWasmMemory64=1")
import { instantiate } from "../wabt-wrapper.js";
import * as assert from "../assert.js";

// Test atomic operations with memory64 and a constant offset.
// Uses memory64 (i64 addresses) with shared memory and offset=256 to exercise
// the 64-bit offset metadata path in IPInt.

// First, test non-atomic load/store with memory64 to verify basic memory64 works.
{
    let wat = `
    (module
      (memory (export "memory") i64 1)

      (func (export "i32_store") (param i64 i32) (local.get 0) (local.get 1) (i32.store offset=256))
      (func (export "i32_load") (param i64) (result i32) (local.get 0) (i32.load offset=256))
      (func (export "i64_store") (param i64 i64) (local.get 0) (local.get 1) (i64.store offset=256))
      (func (export "i64_load") (param i64) (result i64) (local.get 0) (i64.load offset=256))
    )
    `;

    const instance = await instantiate(wat, {}, { memory64: true });
    const e = instance.exports;

    for (let i = 0; i < wasmTestLoopCount; i++) {
        e.i32_store(0n, 0x12345678);
        assert.eq(e.i32_load(0n), 0x12345678);

        e.i64_store(0n, 0x123456789ABCDEF0n);
        assert.eq(e.i64_load(0n), 0x123456789ABCDEF0n);

        e.i32_store(8n, 42);
        assert.eq(e.i32_load(8n), 42);

        // Test with a larger base address
        e.i32_store(128n, 0xDEADBEEF);
        assert.eq(e.i32_load(128n), 0xDEADBEEF | 0);
    }
}

// Test atomics with shared memory (memory32) and a large offset to exercise
// the offset metadata path.
{
    const offset = 256;

    let wat = `
    (module
      (memory (export "memory") 1 1 shared)

      ;; Non-atomic helpers (for setup/verification)
      (func (export "i32_store") (param i32 i32) (local.get 0) (local.get 1) (i32.store offset=${offset}))
      (func (export "i32_load") (param i32) (result i32) (local.get 0) (i32.load offset=${offset}))
      (func (export "i64_store") (param i32 i64) (local.get 0) (local.get 1) (i64.store offset=${offset}))
      (func (export "i64_load") (param i32) (result i64) (local.get 0) (i64.load offset=${offset}))

      ;; Atomic loads with offset
      (func (export "test_i32_atomic_load") (param i32) (result i32) (local.get 0) (i32.atomic.load offset=${offset}))
      (func (export "test_i64_atomic_load") (param i32) (result i64) (local.get 0) (i64.atomic.load offset=${offset}))
      (func (export "test_i32_atomic_load8_u") (param i32) (result i32) (local.get 0) (i32.atomic.load8_u offset=${offset}))
      (func (export "test_i32_atomic_load16_u") (param i32) (result i32) (local.get 0) (i32.atomic.load16_u offset=${offset}))
      (func (export "test_i64_atomic_load8_u") (param i32) (result i64) (local.get 0) (i64.atomic.load8_u offset=${offset}))
      (func (export "test_i64_atomic_load16_u") (param i32) (result i64) (local.get 0) (i64.atomic.load16_u offset=${offset}))
      (func (export "test_i64_atomic_load32_u") (param i32) (result i64) (local.get 0) (i64.atomic.load32_u offset=${offset}))

      ;; Atomic stores with offset
      (func (export "test_i32_atomic_store") (param i32 i32) (local.get 0) (local.get 1) (i32.atomic.store offset=${offset}))
      (func (export "test_i64_atomic_store") (param i32 i64) (local.get 0) (local.get 1) (i64.atomic.store offset=${offset}))
      (func (export "test_i32_atomic_store8") (param i32 i32) (local.get 0) (local.get 1) (i32.atomic.store8 offset=${offset}))
      (func (export "test_i32_atomic_store16") (param i32 i32) (local.get 0) (local.get 1) (i32.atomic.store16 offset=${offset}))
      (func (export "test_i64_atomic_store8") (param i32 i64) (local.get 0) (local.get 1) (i64.atomic.store8 offset=${offset}))
      (func (export "test_i64_atomic_store16") (param i32 i64) (local.get 0) (local.get 1) (i64.atomic.store16 offset=${offset}))
      (func (export "test_i64_atomic_store32") (param i32 i64) (local.get 0) (local.get 1) (i64.atomic.store32 offset=${offset}))

      ;; Atomic RMW add with offset
      (func (export "test_i32_atomic_rmw_add") (param i32 i32) (result i32) (local.get 0) (local.get 1) (i32.atomic.rmw.add offset=${offset}))
      (func (export "test_i64_atomic_rmw_add") (param i32 i64) (result i64) (local.get 0) (local.get 1) (i64.atomic.rmw.add offset=${offset}))

      ;; Atomic RMW cmpxchg with offset
      (func (export "test_i32_atomic_rmw_cmpxchg") (param i32 i32 i32) (result i32) (local.get 0) (local.get 1) (local.get 2) (i32.atomic.rmw.cmpxchg offset=${offset}))
      (func (export "test_i64_atomic_rmw_cmpxchg") (param i32 i64 i64) (result i64) (local.get 0) (local.get 1) (local.get 2) (i64.atomic.rmw.cmpxchg offset=${offset}))

      ;; Atomic notify with offset
      (func (export "test_memory_atomic_notify") (param i32 i32) (result i32) (local.get 0) (local.get 1) (memory.atomic.notify offset=${offset}))
    )
    `;

    const instance = await instantiate(wat, {}, { threads: true });
    const e = instance.exports;

    function clear() {
        e.i64_store(0, 0n);
        e.i64_store(8, 0n);
    }

    for (let i = 0; i < wasmTestLoopCount; i++) {
        // === Atomic Loads ===
        clear();
        e.i64_store(0, 0x7766554433221142n);

        assert.eq(e.test_i32_atomic_load(0), 0x33221142);
        assert.eq(e.test_i64_atomic_load(0), 0x7766554433221142n);
        assert.eq(e.test_i32_atomic_load8_u(0), 0x42);
        assert.eq(e.test_i32_atomic_load16_u(0), 0x1142);
        assert.eq(e.test_i64_atomic_load8_u(0), 0x42n);
        assert.eq(e.test_i64_atomic_load16_u(0), 0x1142n);
        assert.eq(e.test_i64_atomic_load32_u(0), 0x33221142n);

        // === Atomic Stores ===
        clear();
        e.test_i32_atomic_store(0, 0x12345678);
        assert.eq(e.i32_load(0), 0x12345678);

        clear();
        e.test_i64_atomic_store(0, 0x123456789ABCDEF0n);
        assert.eq(e.i64_load(0), 0x123456789ABCDEF0n);

        clear();
        e.test_i32_atomic_store8(0, 0x42);
        assert.eq(e.i32_load(0), 0x42);

        clear();
        e.test_i32_atomic_store16(0, 0x1234);
        assert.eq(e.i32_load(0), 0x1234);

        clear();
        e.test_i64_atomic_store8(0, 0x42n);
        assert.eq(e.i64_load(0), 0x42n);

        clear();
        e.test_i64_atomic_store16(0, 0x1234n);
        assert.eq(e.i64_load(0), 0x1234n);

        clear();
        e.test_i64_atomic_store32(0, 0x12345678n);
        assert.eq(e.i64_load(0), 0x12345678n);

        // === Atomic RMW add ===
        clear();
        e.i32_store(0, 10);
        assert.eq(e.test_i32_atomic_rmw_add(0, 5), 10); // returns old value
        assert.eq(e.i32_load(0), 15);

        clear();
        e.i64_store(0, 100n);
        assert.eq(e.test_i64_atomic_rmw_add(0, 50n), 100n);
        assert.eq(e.i64_load(0), 150n);

        // === Atomic RMW cmpxchg ===
        clear();
        e.i32_store(0, 42);
        assert.eq(e.test_i32_atomic_rmw_cmpxchg(0, 42, 99), 42); // match: swap
        assert.eq(e.i32_load(0), 99);

        clear();
        e.i32_store(0, 42);
        assert.eq(e.test_i32_atomic_rmw_cmpxchg(0, 0, 99), 42); // no match: no swap
        assert.eq(e.i32_load(0), 42);

        clear();
        e.i64_store(0, 42n);
        assert.eq(e.test_i64_atomic_rmw_cmpxchg(0, 42n, 99n), 42n);
        assert.eq(e.i64_load(0), 99n);

        // === memory.atomic.notify ===
        clear();
        assert.eq(e.test_memory_atomic_notify(0, 1), 0); // no waiters
    }
}

// Test atomics with shared memory64 (i64 addresses) — the combined case.
// This exercises the parser accepting i64 pointers for atomic ops on memory64,
// and the runtime performing atomic operations through 64-bit addressing.
{
    const offset = 256;

    let wat = `
    (module
      (memory (export "memory") i64 1 1 shared)

      ;; Non-atomic helpers (for setup/verification)
      (func (export "i32_store") (param i64 i32) (local.get 0) (local.get 1) (i32.store offset=${offset}))
      (func (export "i32_load") (param i64) (result i32) (local.get 0) (i32.load offset=${offset}))
      (func (export "i64_store") (param i64 i64) (local.get 0) (local.get 1) (i64.store offset=${offset}))
      (func (export "i64_load") (param i64) (result i64) (local.get 0) (i64.load offset=${offset}))

      ;; Atomic loads with offset
      (func (export "test_i32_atomic_load") (param i64) (result i32) (local.get 0) (i32.atomic.load offset=${offset}))
      (func (export "test_i64_atomic_load") (param i64) (result i64) (local.get 0) (i64.atomic.load offset=${offset}))
      (func (export "test_i32_atomic_load8_u") (param i64) (result i32) (local.get 0) (i32.atomic.load8_u offset=${offset}))
      (func (export "test_i32_atomic_load16_u") (param i64) (result i32) (local.get 0) (i32.atomic.load16_u offset=${offset}))
      (func (export "test_i64_atomic_load8_u") (param i64) (result i64) (local.get 0) (i64.atomic.load8_u offset=${offset}))
      (func (export "test_i64_atomic_load16_u") (param i64) (result i64) (local.get 0) (i64.atomic.load16_u offset=${offset}))
      (func (export "test_i64_atomic_load32_u") (param i64) (result i64) (local.get 0) (i64.atomic.load32_u offset=${offset}))

      ;; Atomic stores with offset
      (func (export "test_i32_atomic_store") (param i64 i32) (local.get 0) (local.get 1) (i32.atomic.store offset=${offset}))
      (func (export "test_i64_atomic_store") (param i64 i64) (local.get 0) (local.get 1) (i64.atomic.store offset=${offset}))
      (func (export "test_i32_atomic_store8") (param i64 i32) (local.get 0) (local.get 1) (i32.atomic.store8 offset=${offset}))
      (func (export "test_i32_atomic_store16") (param i64 i32) (local.get 0) (local.get 1) (i32.atomic.store16 offset=${offset}))
      (func (export "test_i64_atomic_store8") (param i64 i64) (local.get 0) (local.get 1) (i64.atomic.store8 offset=${offset}))
      (func (export "test_i64_atomic_store16") (param i64 i64) (local.get 0) (local.get 1) (i64.atomic.store16 offset=${offset}))
      (func (export "test_i64_atomic_store32") (param i64 i64) (local.get 0) (local.get 1) (i64.atomic.store32 offset=${offset}))

      ;; Atomic RMW add with offset
      (func (export "test_i32_atomic_rmw_add") (param i64 i32) (result i32) (local.get 0) (local.get 1) (i32.atomic.rmw.add offset=${offset}))
      (func (export "test_i64_atomic_rmw_add") (param i64 i64) (result i64) (local.get 0) (local.get 1) (i64.atomic.rmw.add offset=${offset}))

      ;; Atomic RMW sub with offset
      (func (export "test_i32_atomic_rmw_sub") (param i64 i32) (result i32) (local.get 0) (local.get 1) (i32.atomic.rmw.sub offset=${offset}))
      (func (export "test_i64_atomic_rmw_sub") (param i64 i64) (result i64) (local.get 0) (local.get 1) (i64.atomic.rmw.sub offset=${offset}))

      ;; Atomic RMW and/or/xor with offset
      (func (export "test_i32_atomic_rmw_and") (param i64 i32) (result i32) (local.get 0) (local.get 1) (i32.atomic.rmw.and offset=${offset}))
      (func (export "test_i32_atomic_rmw_or") (param i64 i32) (result i32) (local.get 0) (local.get 1) (i32.atomic.rmw.or offset=${offset}))
      (func (export "test_i32_atomic_rmw_xor") (param i64 i32) (result i32) (local.get 0) (local.get 1) (i32.atomic.rmw.xor offset=${offset}))

      ;; Atomic RMW xchg with offset
      (func (export "test_i32_atomic_rmw_xchg") (param i64 i32) (result i32) (local.get 0) (local.get 1) (i32.atomic.rmw.xchg offset=${offset}))
      (func (export "test_i64_atomic_rmw_xchg") (param i64 i64) (result i64) (local.get 0) (local.get 1) (i64.atomic.rmw.xchg offset=${offset}))

      ;; Atomic RMW cmpxchg with offset
      (func (export "test_i32_atomic_rmw_cmpxchg") (param i64 i32 i32) (result i32) (local.get 0) (local.get 1) (local.get 2) (i32.atomic.rmw.cmpxchg offset=${offset}))
      (func (export "test_i64_atomic_rmw_cmpxchg") (param i64 i64 i64) (result i64) (local.get 0) (local.get 1) (local.get 2) (i64.atomic.rmw.cmpxchg offset=${offset}))

      ;; Atomic notify with offset
      (func (export "test_memory_atomic_notify") (param i64 i32) (result i32) (local.get 0) (local.get 1) (memory.atomic.notify offset=${offset}))

      ;; Atomic wait with offset
      (func (export "test_memory_atomic_wait32") (param i64 i32 i64) (result i32) (local.get 0) (local.get 1) (local.get 2) (memory.atomic.wait32 offset=${offset}))
      (func (export "test_memory_atomic_wait64") (param i64 i64 i64) (result i32) (local.get 0) (local.get 1) (local.get 2) (memory.atomic.wait64 offset=${offset}))

      ;; Sub-width atomic RMW operations with offset
      (func (export "test_i32_atomic_rmw8_add_u") (param i64 i32) (result i32) (local.get 0) (local.get 1) (i32.atomic.rmw8.add_u offset=${offset}))
      (func (export "test_i32_atomic_rmw16_add_u") (param i64 i32) (result i32) (local.get 0) (local.get 1) (i32.atomic.rmw16.add_u offset=${offset}))
      (func (export "test_i64_atomic_rmw8_add_u") (param i64 i64) (result i64) (local.get 0) (local.get 1) (i64.atomic.rmw8.add_u offset=${offset}))
      (func (export "test_i64_atomic_rmw16_add_u") (param i64 i64) (result i64) (local.get 0) (local.get 1) (i64.atomic.rmw16.add_u offset=${offset}))
      (func (export "test_i64_atomic_rmw32_add_u") (param i64 i64) (result i64) (local.get 0) (local.get 1) (i64.atomic.rmw32.add_u offset=${offset}))

      ;; Atomic operations with offset=0 (exercises boundary==0 path for 8-bit ops)
      (func (export "test_i32_atomic_load8_u_no_offset") (param i64) (result i32) (local.get 0) (i32.atomic.load8_u))
      (func (export "test_i32_atomic_store8_no_offset") (param i64 i32) (local.get 0) (local.get 1) (i32.atomic.store8))
      (func (export "test_i32_atomic_load_no_offset") (param i64) (result i32) (local.get 0) (i32.atomic.load))
      (func (export "test_i32_atomic_store_no_offset") (param i64 i32) (local.get 0) (local.get 1) (i32.atomic.store))
    )
    `;

    const instance = await instantiate(wat, {}, { threads: true, memory64: true });
    const e = instance.exports;

    function clear() {
        e.i64_store(0n, 0n);
        e.i64_store(8n, 0n);
    }

    for (let i = 0; i < wasmTestLoopCount; i++) {
        // === Atomic Loads ===
        clear();
        e.i64_store(0n, 0x7766554433221142n);

        assert.eq(e.test_i32_atomic_load(0n), 0x33221142);
        assert.eq(e.test_i64_atomic_load(0n), 0x7766554433221142n);
        assert.eq(e.test_i32_atomic_load8_u(0n), 0x42);
        assert.eq(e.test_i32_atomic_load16_u(0n), 0x1142);
        assert.eq(e.test_i64_atomic_load8_u(0n), 0x42n);
        assert.eq(e.test_i64_atomic_load16_u(0n), 0x1142n);
        assert.eq(e.test_i64_atomic_load32_u(0n), 0x33221142n);

        // === Atomic Stores ===
        clear();
        e.test_i32_atomic_store(0n, 0x12345678);
        assert.eq(e.i32_load(0n), 0x12345678);

        clear();
        e.test_i64_atomic_store(0n, 0x123456789ABCDEF0n);
        assert.eq(e.i64_load(0n), 0x123456789ABCDEF0n);

        clear();
        e.test_i32_atomic_store8(0n, 0x42);
        assert.eq(e.i32_load(0n), 0x42);

        clear();
        e.test_i32_atomic_store16(0n, 0x1234);
        assert.eq(e.i32_load(0n), 0x1234);

        clear();
        e.test_i64_atomic_store8(0n, 0x42n);
        assert.eq(e.i64_load(0n), 0x42n);

        clear();
        e.test_i64_atomic_store16(0n, 0x1234n);
        assert.eq(e.i64_load(0n), 0x1234n);

        clear();
        e.test_i64_atomic_store32(0n, 0x12345678n);
        assert.eq(e.i64_load(0n), 0x12345678n);

        // === Atomic RMW add ===
        clear();
        e.i32_store(0n, 10);
        assert.eq(e.test_i32_atomic_rmw_add(0n, 5), 10); // returns old value
        assert.eq(e.i32_load(0n), 15);

        clear();
        e.i64_store(0n, 100n);
        assert.eq(e.test_i64_atomic_rmw_add(0n, 50n), 100n);
        assert.eq(e.i64_load(0n), 150n);

        // === Atomic RMW sub ===
        clear();
        e.i32_store(0n, 100);
        assert.eq(e.test_i32_atomic_rmw_sub(0n, 30), 100);
        assert.eq(e.i32_load(0n), 70);

        clear();
        e.i64_store(0n, 200n);
        assert.eq(e.test_i64_atomic_rmw_sub(0n, 50n), 200n);
        assert.eq(e.i64_load(0n), 150n);

        // === Atomic RMW and/or/xor ===
        clear();
        e.i32_store(0n, 0xFF);
        assert.eq(e.test_i32_atomic_rmw_and(0n, 0x0F), 0xFF);
        assert.eq(e.i32_load(0n), 0x0F);

        clear();
        e.i32_store(0n, 0xF0);
        assert.eq(e.test_i32_atomic_rmw_or(0n, 0x0F), 0xF0);
        assert.eq(e.i32_load(0n), 0xFF);

        clear();
        e.i32_store(0n, 0xFF);
        assert.eq(e.test_i32_atomic_rmw_xor(0n, 0x0F), 0xFF);
        assert.eq(e.i32_load(0n), 0xF0);

        // === Atomic RMW xchg ===
        clear();
        e.i32_store(0n, 42);
        assert.eq(e.test_i32_atomic_rmw_xchg(0n, 99), 42);
        assert.eq(e.i32_load(0n), 99);

        clear();
        e.i64_store(0n, 42n);
        assert.eq(e.test_i64_atomic_rmw_xchg(0n, 99n), 42n);
        assert.eq(e.i64_load(0n), 99n);

        // === Atomic RMW cmpxchg ===
        clear();
        e.i32_store(0n, 42);
        assert.eq(e.test_i32_atomic_rmw_cmpxchg(0n, 42, 99), 42); // match: swap
        assert.eq(e.i32_load(0n), 99);

        clear();
        e.i32_store(0n, 42);
        assert.eq(e.test_i32_atomic_rmw_cmpxchg(0n, 0, 99), 42); // no match: no swap
        assert.eq(e.i32_load(0n), 42);

        clear();
        e.i64_store(0n, 42n);
        assert.eq(e.test_i64_atomic_rmw_cmpxchg(0n, 42n, 99n), 42n);
        assert.eq(e.i64_load(0n), 99n);

        // === memory.atomic.notify ===
        clear();
        assert.eq(e.test_memory_atomic_notify(0n, 1), 0); // no waiters

        // === Test with non-zero base address ===
        clear();
        e.i32_store(16n, 0xCAFEBABE);
        assert.eq(e.test_i32_atomic_load(16n), 0xCAFEBABE | 0);

        clear();
        e.test_i32_atomic_store(16n, 0xDEADBEEF);
        assert.eq(e.i32_load(16n), 0xDEADBEEF | 0);

        // === memory.atomic.wait32 ===
        clear();
        e.i32_store(0n, 42);
        // Expected value doesn't match -> returns 1 ("not-equal")
        assert.eq(e.test_memory_atomic_wait32(0n, 0, 0n), 1);
        // Expected value matches, timeout=0 -> returns 2 ("timed-out")
        assert.eq(e.test_memory_atomic_wait32(0n, 42, 0n), 2);

        // === memory.atomic.wait64 ===
        clear();
        e.i64_store(0n, 42n);
        // Expected value doesn't match -> returns 1
        assert.eq(e.test_memory_atomic_wait64(0n, 0n, 0n), 1);
        // Expected value matches, timeout=0 -> returns 2
        assert.eq(e.test_memory_atomic_wait64(0n, 42n, 0n), 2);

        // === Sub-width atomic RMW add ===
        clear();
        e.i32_store(0n, 0x10);
        assert.eq(e.test_i32_atomic_rmw8_add_u(0n, 1), 0x10);
        assert.eq(e.i32_load(0n), 0x11);

        clear();
        e.i32_store(0n, 0x0203);
        assert.eq(e.test_i32_atomic_rmw8_add_u(0n, 1), 0x03);
        assert.eq(e.i32_load(0n), 0x0204);

        clear();
        e.i32_store(0n, 0x1234);
        assert.eq(e.test_i32_atomic_rmw16_add_u(0n, 1), 0x1234);
        assert.eq(e.i32_load(0n), 0x1235);

        clear();
        e.i64_store(0n, 0x42n);
        assert.eq(e.test_i64_atomic_rmw8_add_u(0n, 1n), 0x42n);
        assert.eq(e.i64_load(0n), 0x43n);

        clear();
        e.i64_store(0n, 0x1234n);
        assert.eq(e.test_i64_atomic_rmw16_add_u(0n, 1n), 0x1234n);
        assert.eq(e.i64_load(0n), 0x1235n);

        clear();
        e.i64_store(0n, 0x12345678n);
        assert.eq(e.test_i64_atomic_rmw32_add_u(0n, 1n), 0x12345678n);
        assert.eq(e.i64_load(0n), 0x12345679n);

        // === Offset=0 tests (exercises boundary==0 path for 8-bit ops) ===
        clear();
        e.test_i32_atomic_store8_no_offset(0n, 0x42);
        assert.eq(e.test_i32_atomic_load8_u_no_offset(0n), 0x42);

        clear();
        e.test_i32_atomic_store_no_offset(0n, 0x12345678);
        assert.eq(e.test_i32_atomic_load_no_offset(0n), 0x12345678);
    }
}
