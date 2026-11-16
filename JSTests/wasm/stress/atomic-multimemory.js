//@ requireOptions("--useWasmMultiMemory=1")

import * as assert from "../assert.js";
import { instantiate } from "../wabt-wrapper.js";

// Test all atomic instructions that take a memory index with multi-memory

let wat = `
(module
  (import "js" "memory0" (memory 1 1 shared))
  (import "js" "memory1" (memory 1 1 shared))

  ;; Non-atomic helpers for memory 1 (setup/verification)
  (func (export "i32_load") (param i32) (result i32) (local.get 0) (i32.load 1))
  (func (export "i32_store") (param i32 i32) (local.get 0) (local.get 1) (i32.store 1))
  (func (export "i64_load") (param i32) (result i64) (local.get 0) (i64.load 1))
  (func (export "i64_store") (param i32 i64) (local.get 0) (local.get 1) (i64.store 1))


  ;; memory.atomic.notify / wait
  (func (export "test_memory_atomic_notify") (param i32 i32) (result i32)
    (local.get 0) (local.get 1) (memory.atomic.notify 1))
  (func (export "test_memory_atomic_wait32") (param i32 i32 i64) (result i32)
    (local.get 0) (local.get 1) (local.get 2) (memory.atomic.wait32 1))
  (func (export "test_memory_atomic_wait64") (param i32 i64 i64) (result i32)
    (local.get 0) (local.get 1) (local.get 2) (memory.atomic.wait64 1))

  ;; Atomic loads from memory 1
  (func (export "test_i32_atomic_load") (param i32) (result i32) (local.get 0) (i32.atomic.load 1))
  (func (export "test_i64_atomic_load") (param i32) (result i64) (local.get 0) (i64.atomic.load 1))
  (func (export "test_i32_atomic_load8_u") (param i32) (result i32) (local.get 0) (i32.atomic.load8_u 1))
  (func (export "test_i32_atomic_load16_u") (param i32) (result i32) (local.get 0) (i32.atomic.load16_u 1))
  (func (export "test_i64_atomic_load8_u") (param i32) (result i64) (local.get 0) (i64.atomic.load8_u 1))
  (func (export "test_i64_atomic_load16_u") (param i32) (result i64) (local.get 0) (i64.atomic.load16_u 1))
  (func (export "test_i64_atomic_load32_u") (param i32) (result i64) (local.get 0) (i64.atomic.load32_u 1))

  ;; Atomic stores to memory 1
  (func (export "test_i32_atomic_store") (param i32 i32) (local.get 0) (local.get 1) (i32.atomic.store 1))
  (func (export "test_i64_atomic_store") (param i32 i64) (local.get 0) (local.get 1) (i64.atomic.store 1))
  (func (export "test_i32_atomic_store8") (param i32 i32) (local.get 0) (local.get 1) (i32.atomic.store8 1))
  (func (export "test_i32_atomic_store16") (param i32 i32) (local.get 0) (local.get 1) (i32.atomic.store16 1))
  (func (export "test_i64_atomic_store8") (param i32 i64) (local.get 0) (local.get 1) (i64.atomic.store8 1))
  (func (export "test_i64_atomic_store16") (param i32 i64) (local.get 0) (local.get 1) (i64.atomic.store16 1))
  (func (export "test_i64_atomic_store32") (param i32 i64) (local.get 0) (local.get 1) (i64.atomic.store32 1))

  ;; Atomic RMW add (returns old value)
  (func (export "test_i32_atomic_rmw_add") (param i32 i32) (result i32) (local.get 0) (local.get 1) (i32.atomic.rmw.add 1))
  (func (export "test_i64_atomic_rmw_add") (param i32 i64) (result i64) (local.get 0) (local.get 1) (i64.atomic.rmw.add 1))
  (func (export "test_i32_atomic_rmw8_add_u") (param i32 i32) (result i32) (local.get 0) (local.get 1) (i32.atomic.rmw8.add_u 1))
  (func (export "test_i32_atomic_rmw16_add_u") (param i32 i32) (result i32) (local.get 0) (local.get 1) (i32.atomic.rmw16.add_u 1))
  (func (export "test_i64_atomic_rmw8_add_u") (param i32 i64) (result i64) (local.get 0) (local.get 1) (i64.atomic.rmw8.add_u 1))
  (func (export "test_i64_atomic_rmw16_add_u") (param i32 i64) (result i64) (local.get 0) (local.get 1) (i64.atomic.rmw16.add_u 1))
  (func (export "test_i64_atomic_rmw32_add_u") (param i32 i64) (result i64) (local.get 0) (local.get 1) (i64.atomic.rmw32.add_u 1))

  ;; Atomic RMW sub
  (func (export "test_i32_atomic_rmw_sub") (param i32 i32) (result i32) (local.get 0) (local.get 1) (i32.atomic.rmw.sub 1))
  (func (export "test_i64_atomic_rmw_sub") (param i32 i64) (result i64) (local.get 0) (local.get 1) (i64.atomic.rmw.sub 1))
  (func (export "test_i32_atomic_rmw8_sub_u") (param i32 i32) (result i32) (local.get 0) (local.get 1) (i32.atomic.rmw8.sub_u 1))
  (func (export "test_i32_atomic_rmw16_sub_u") (param i32 i32) (result i32) (local.get 0) (local.get 1) (i32.atomic.rmw16.sub_u 1))
  (func (export "test_i64_atomic_rmw8_sub_u") (param i32 i64) (result i64) (local.get 0) (local.get 1) (i64.atomic.rmw8.sub_u 1))
  (func (export "test_i64_atomic_rmw16_sub_u") (param i32 i64) (result i64) (local.get 0) (local.get 1) (i64.atomic.rmw16.sub_u 1))
  (func (export "test_i64_atomic_rmw32_sub_u") (param i32 i64) (result i64) (local.get 0) (local.get 1) (i64.atomic.rmw32.sub_u 1))

  ;; Atomic RMW and
  (func (export "test_i32_atomic_rmw_and") (param i32 i32) (result i32) (local.get 0) (local.get 1) (i32.atomic.rmw.and 1))
  (func (export "test_i64_atomic_rmw_and") (param i32 i64) (result i64) (local.get 0) (local.get 1) (i64.atomic.rmw.and 1))
  (func (export "test_i32_atomic_rmw8_and_u") (param i32 i32) (result i32) (local.get 0) (local.get 1) (i32.atomic.rmw8.and_u 1))
  (func (export "test_i32_atomic_rmw16_and_u") (param i32 i32) (result i32) (local.get 0) (local.get 1) (i32.atomic.rmw16.and_u 1))
  (func (export "test_i64_atomic_rmw8_and_u") (param i32 i64) (result i64) (local.get 0) (local.get 1) (i64.atomic.rmw8.and_u 1))
  (func (export "test_i64_atomic_rmw16_and_u") (param i32 i64) (result i64) (local.get 0) (local.get 1) (i64.atomic.rmw16.and_u 1))
  (func (export "test_i64_atomic_rmw32_and_u") (param i32 i64) (result i64) (local.get 0) (local.get 1) (i64.atomic.rmw32.and_u 1))

  ;; Atomic RMW or
  (func (export "test_i32_atomic_rmw_or") (param i32 i32) (result i32) (local.get 0) (local.get 1) (i32.atomic.rmw.or 1))
  (func (export "test_i64_atomic_rmw_or") (param i32 i64) (result i64) (local.get 0) (local.get 1) (i64.atomic.rmw.or 1))
  (func (export "test_i32_atomic_rmw8_or_u") (param i32 i32) (result i32) (local.get 0) (local.get 1) (i32.atomic.rmw8.or_u 1))
  (func (export "test_i32_atomic_rmw16_or_u") (param i32 i32) (result i32) (local.get 0) (local.get 1) (i32.atomic.rmw16.or_u 1))
  (func (export "test_i64_atomic_rmw8_or_u") (param i32 i64) (result i64) (local.get 0) (local.get 1) (i64.atomic.rmw8.or_u 1))
  (func (export "test_i64_atomic_rmw16_or_u") (param i32 i64) (result i64) (local.get 0) (local.get 1) (i64.atomic.rmw16.or_u 1))
  (func (export "test_i64_atomic_rmw32_or_u") (param i32 i64) (result i64) (local.get 0) (local.get 1) (i64.atomic.rmw32.or_u 1))

  ;; Atomic RMW xor
  (func (export "test_i32_atomic_rmw_xor") (param i32 i32) (result i32) (local.get 0) (local.get 1) (i32.atomic.rmw.xor 1))
  (func (export "test_i64_atomic_rmw_xor") (param i32 i64) (result i64) (local.get 0) (local.get 1) (i64.atomic.rmw.xor 1))
  (func (export "test_i32_atomic_rmw8_xor_u") (param i32 i32) (result i32) (local.get 0) (local.get 1) (i32.atomic.rmw8.xor_u 1))
  (func (export "test_i32_atomic_rmw16_xor_u") (param i32 i32) (result i32) (local.get 0) (local.get 1) (i32.atomic.rmw16.xor_u 1))
  (func (export "test_i64_atomic_rmw8_xor_u") (param i32 i64) (result i64) (local.get 0) (local.get 1) (i64.atomic.rmw8.xor_u 1))
  (func (export "test_i64_atomic_rmw16_xor_u") (param i32 i64) (result i64) (local.get 0) (local.get 1) (i64.atomic.rmw16.xor_u 1))
  (func (export "test_i64_atomic_rmw32_xor_u") (param i32 i64) (result i64) (local.get 0) (local.get 1) (i64.atomic.rmw32.xor_u 1))

  ;; Atomic RMW xchg
  (func (export "test_i32_atomic_rmw_xchg") (param i32 i32) (result i32) (local.get 0) (local.get 1) (i32.atomic.rmw.xchg 1))
  (func (export "test_i64_atomic_rmw_xchg") (param i32 i64) (result i64) (local.get 0) (local.get 1) (i64.atomic.rmw.xchg 1))
  (func (export "test_i32_atomic_rmw8_xchg_u") (param i32 i32) (result i32) (local.get 0) (local.get 1) (i32.atomic.rmw8.xchg_u 1))
  (func (export "test_i32_atomic_rmw16_xchg_u") (param i32 i32) (result i32) (local.get 0) (local.get 1) (i32.atomic.rmw16.xchg_u 1))
  (func (export "test_i64_atomic_rmw8_xchg_u") (param i32 i64) (result i64) (local.get 0) (local.get 1) (i64.atomic.rmw8.xchg_u 1))
  (func (export "test_i64_atomic_rmw16_xchg_u") (param i32 i64) (result i64) (local.get 0) (local.get 1) (i64.atomic.rmw16.xchg_u 1))
  (func (export "test_i64_atomic_rmw32_xchg_u") (param i32 i64) (result i64) (local.get 0) (local.get 1) (i64.atomic.rmw32.xchg_u 1))

  ;; Atomic RMW cmpxchg (address, expected, replacement -> old value)
  (func (export "test_i32_atomic_rmw_cmpxchg") (param i32 i32 i32) (result i32) (local.get 0) (local.get 1) (local.get 2) (i32.atomic.rmw.cmpxchg 1))
  (func (export "test_i64_atomic_rmw_cmpxchg") (param i32 i64 i64) (result i64) (local.get 0) (local.get 1) (local.get 2) (i64.atomic.rmw.cmpxchg 1))
  (func (export "test_i32_atomic_rmw8_cmpxchg_u") (param i32 i32 i32) (result i32) (local.get 0) (local.get 1) (local.get 2) (i32.atomic.rmw8.cmpxchg_u 1))
  (func (export "test_i32_atomic_rmw16_cmpxchg_u") (param i32 i32 i32) (result i32) (local.get 0) (local.get 1) (local.get 2) (i32.atomic.rmw16.cmpxchg_u 1))
  (func (export "test_i64_atomic_rmw8_cmpxchg_u") (param i32 i64 i64) (result i64) (local.get 0) (local.get 1) (local.get 2) (i64.atomic.rmw8.cmpxchg_u 1))
  (func (export "test_i64_atomic_rmw16_cmpxchg_u") (param i32 i64 i64) (result i64) (local.get 0) (local.get 1) (local.get 2) (i64.atomic.rmw16.cmpxchg_u 1))
  (func (export "test_i64_atomic_rmw32_cmpxchg_u") (param i32 i64 i64) (result i64) (local.get 0) (local.get 1) (local.get 2) (i64.atomic.rmw32.cmpxchg_u 1))
)
`;

async function test() {
    const mem0 = new WebAssembly.Memory({ initial: 1, maximum: 1, shared: true });
    const mem1 = new WebAssembly.Memory({ initial: 1, maximum: 1, shared: true });

    const instance = await instantiate(wat, { js: { memory0: mem0, memory1: mem1 } }, { threads: true, multi_memory: true });
    const e = instance.exports;

    function clear() {
        e.i64_store(0, 0n);
        e.i64_store(8, 0n);
    }

    function testRMWi32(name, initial, operand, expectedOld, expectedNew) {
        clear();
        e.i32_store(0, initial);
        assert.eq(e[`test_${name}`](0, operand), expectedOld);
        assert.eq(e.i32_load(0), expectedNew);
    }

    function testRMWi64(name, initial, operand, expectedOld, expectedNew) {
        clear();
        e.i64_store(0, initial);
        assert.eq(e[`test_${name}`](0, operand), expectedOld);
        assert.eq(e.i64_load(0), expectedNew);
    }

    function testCmpxchgi32(name, initial, expected, replacement, expectedOld, expectedNew) {
        clear();
        e.i32_store(0, initial);
        assert.eq(e[`test_${name}`](0, expected, replacement), expectedOld);
        assert.eq(e.i32_load(0), expectedNew);
    }

    function testCmpxchgi64(name, initial, expected, replacement, expectedOld, expectedNew) {
        clear();
        e.i64_store(0, initial);
        assert.eq(e[`test_${name}`](0, expected, replacement), expectedOld);
        assert.eq(e.i64_load(0), expectedNew);
    }

    for (let i = 0; i < wasmTestLoopCount; i++) {
        // === memory.atomic.notify ===
        clear();
        assert.eq(e.test_memory_atomic_notify(0, 1), 0); // no waiters

        // memory.atomic.wait{32,64} are not tested here, but they are handled
        // by external C calls anyway which have not changed

        // === Atomic Loads ===
        // Non-zero in every byte position so we detect wrong-memory access
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
        testRMWi32("i32_atomic_rmw_add", 10, 5, 10, 15);
        testRMWi64("i64_atomic_rmw_add", 10n, 5n, 10n, 15n);
        testRMWi32("i32_atomic_rmw8_add_u", 10, 5, 10, 15);
        testRMWi32("i32_atomic_rmw16_add_u", 10, 5, 10, 15);
        testRMWi64("i64_atomic_rmw8_add_u", 10n, 5n, 10n, 15n);
        testRMWi64("i64_atomic_rmw16_add_u", 10n, 5n, 10n, 15n);
        testRMWi64("i64_atomic_rmw32_add_u", 10n, 5n, 10n, 15n);

        // === Atomic RMW sub ===
        testRMWi32("i32_atomic_rmw_sub", 10, 3, 10, 7);
        testRMWi64("i64_atomic_rmw_sub", 10n, 3n, 10n, 7n);
        testRMWi32("i32_atomic_rmw8_sub_u", 10, 3, 10, 7);
        testRMWi32("i32_atomic_rmw16_sub_u", 10, 3, 10, 7);
        testRMWi64("i64_atomic_rmw8_sub_u", 10n, 3n, 10n, 7n);
        testRMWi64("i64_atomic_rmw16_sub_u", 10n, 3n, 10n, 7n);
        testRMWi64("i64_atomic_rmw32_sub_u", 10n, 3n, 10n, 7n);

        // === Atomic RMW and ===
        testRMWi32("i32_atomic_rmw_and", 0xFF, 0x0F, 0xFF, 0x0F);
        testRMWi64("i64_atomic_rmw_and", 0xFFn, 0x0Fn, 0xFFn, 0x0Fn);
        testRMWi32("i32_atomic_rmw8_and_u", 0xFF, 0x0F, 0xFF, 0x0F);
        testRMWi32("i32_atomic_rmw16_and_u", 0xFF, 0x0F, 0xFF, 0x0F);
        testRMWi64("i64_atomic_rmw8_and_u", 0xFFn, 0x0Fn, 0xFFn, 0x0Fn);
        testRMWi64("i64_atomic_rmw16_and_u", 0xFFn, 0x0Fn, 0xFFn, 0x0Fn);
        testRMWi64("i64_atomic_rmw32_and_u", 0xFFn, 0x0Fn, 0xFFn, 0x0Fn);

        // === Atomic RMW or ===
        testRMWi32("i32_atomic_rmw_or", 0x0F, 0xF0, 0x0F, 0xFF);
        testRMWi64("i64_atomic_rmw_or", 0x0Fn, 0xF0n, 0x0Fn, 0xFFn);
        testRMWi32("i32_atomic_rmw8_or_u", 0x0F, 0xF0, 0x0F, 0xFF);
        testRMWi32("i32_atomic_rmw16_or_u", 0x0F, 0xF0, 0x0F, 0xFF);
        testRMWi64("i64_atomic_rmw8_or_u", 0x0Fn, 0xF0n, 0x0Fn, 0xFFn);
        testRMWi64("i64_atomic_rmw16_or_u", 0x0Fn, 0xF0n, 0x0Fn, 0xFFn);
        testRMWi64("i64_atomic_rmw32_or_u", 0x0Fn, 0xF0n, 0x0Fn, 0xFFn);

        // === Atomic RMW xor ===
        testRMWi32("i32_atomic_rmw_xor", 0xFF, 0x0F, 0xFF, 0xF0);
        testRMWi64("i64_atomic_rmw_xor", 0xFFn, 0x0Fn, 0xFFn, 0xF0n);
        testRMWi32("i32_atomic_rmw8_xor_u", 0xFF, 0x0F, 0xFF, 0xF0);
        testRMWi32("i32_atomic_rmw16_xor_u", 0xFF, 0x0F, 0xFF, 0xF0);
        testRMWi64("i64_atomic_rmw8_xor_u", 0xFFn, 0x0Fn, 0xFFn, 0xF0n);
        testRMWi64("i64_atomic_rmw16_xor_u", 0xFFn, 0x0Fn, 0xFFn, 0xF0n);
        testRMWi64("i64_atomic_rmw32_xor_u", 0xFFn, 0x0Fn, 0xFFn, 0xF0n);

        // === Atomic RMW xchg ===
        testRMWi32("i32_atomic_rmw_xchg", 42, 99, 42, 99);
        testRMWi64("i64_atomic_rmw_xchg", 42n, 99n, 42n, 99n);
        testRMWi32("i32_atomic_rmw8_xchg_u", 42, 99, 42, 99);
        testRMWi32("i32_atomic_rmw16_xchg_u", 42, 99, 42, 99);
        testRMWi64("i64_atomic_rmw8_xchg_u", 42n, 99n, 42n, 99n);
        testRMWi64("i64_atomic_rmw16_xchg_u", 42n, 99n, 42n, 99n);
        testRMWi64("i64_atomic_rmw32_xchg_u", 42n, 99n, 42n, 99n);

        // === Atomic RMW cmpxchg (success - expected matches) ===
        testCmpxchgi32("i32_atomic_rmw_cmpxchg", 42, 42, 99, 42, 99);
        testCmpxchgi64("i64_atomic_rmw_cmpxchg", 42n, 42n, 99n, 42n, 99n);
        testCmpxchgi32("i32_atomic_rmw8_cmpxchg_u", 42, 42, 99, 42, 99);
        testCmpxchgi32("i32_atomic_rmw16_cmpxchg_u", 42, 42, 99, 42, 99);
        testCmpxchgi64("i64_atomic_rmw8_cmpxchg_u", 42n, 42n, 99n, 42n, 99n);
        testCmpxchgi64("i64_atomic_rmw16_cmpxchg_u", 42n, 42n, 99n, 42n, 99n);
        testCmpxchgi64("i64_atomic_rmw32_cmpxchg_u", 42n, 42n, 99n, 42n, 99n);

        // === Atomic RMW cmpxchg (failure - expected doesn't match) ===
        testCmpxchgi32("i32_atomic_rmw_cmpxchg", 42, 77, 99, 42, 42);
        testCmpxchgi64("i64_atomic_rmw_cmpxchg", 42n, 77n, 99n, 42n, 42n);
        testCmpxchgi32("i32_atomic_rmw8_cmpxchg_u", 42, 77, 99, 42, 42);
        testCmpxchgi32("i32_atomic_rmw16_cmpxchg_u", 42, 77, 99, 42, 42);
        testCmpxchgi64("i64_atomic_rmw8_cmpxchg_u", 42n, 77n, 99n, 42n, 42n);
        testCmpxchgi64("i64_atomic_rmw16_cmpxchg_u", 42n, 77n, 99n, 42n, 42n);
        testCmpxchgi64("i64_atomic_rmw32_cmpxchg_u", 42n, 77n, 99n, 42n, 42n);
    }
}

await assert.asyncTest(test());
