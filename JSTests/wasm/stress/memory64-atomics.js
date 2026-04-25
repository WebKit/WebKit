//@ skip if $addressBits <= 32
//@ runDefaultWasm("-m", "--useWasmMemory64=1", "--useOMGJIT=0")
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
