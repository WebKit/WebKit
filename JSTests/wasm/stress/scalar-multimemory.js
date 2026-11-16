//@ requireOptions("--useWasmMultiMemory=1")

import * as assert from "../assert.js";
import { instantiate } from "../wabt-wrapper.js";

// test scalar instructions that access linear memory

let wat = `
(module
  (import "js" "memory0" (memory 1))
  (import "js" "memory1" (memory 1))

  (func (export "test_load_m1_then_m0") (result i32)
    i32.const 0
    i32.load 1
    i32.const 0
    i32.load 0
    i32.xor
  )

;; loads from memory 0 are already tested elsewhere
  (func (export "i32_load_m1") (param i32) (result i32) (local.get 0) (i32.load 1))
  (func (export "i64_load_m1") (param i32) (result i64) (local.get 0) (i64.load 1))
  (func (export "f32_load_m1") (param i32) (result f32) (local.get 0) (f32.load 1))
  (func (export "f64_load_m1") (param i32) (result f64) (local.get 0) (f64.load 1))

  (func (export "i32_load8s_m1") (param i32) (result i32) (local.get 0) (i32.load8_s 1))
  (func (export "i32_load8u_m1") (param i32) (result i32) (local.get 0) (i32.load8_u 1))
  (func (export "i32_load16s_m1") (param i32) (result i32) (local.get 0) (i32.load16_s 1))
  (func (export "i32_load16u_m1") (param i32) (result i32) (local.get 0) (i32.load16_u 1))

  (func (export "i64_load8s_m1") (param i32) (result i64) (local.get 0) (i64.load8_s 1))
  (func (export "i64_load8u_m1") (param i32) (result i64) (local.get 0) (i64.load8_u 1))
  (func (export "i64_load16s_m1") (param i32) (result i64) (local.get 0) (i64.load16_s 1))
  (func (export "i64_load16u_m1") (param i32) (result i64) (local.get 0) (i64.load16_u 1))
  (func (export "i64_load32s_m1") (param i32) (result i64) (local.get 0) (i64.load32_s 1))
  (func (export "i64_load32u_m1") (param i32) (result i64) (local.get 0) (i64.load32_u 1))

;; stores to memory 0 are already tested elsewhere
  (func (export "test_i32_store") (param i32 i32) (local.get 0) (local.get 1) (i32.store 1))
  (func (export "test_i64_store") (param i32 i64) (local.get 0) (local.get 1) (i64.store 1))
  (func (export "test_f32_store") (param i32 f32) (local.get 0) (local.get 1) (f32.store 1))
  (func (export "test_f64_store") (param i32 f64) (local.get 0) (local.get 1) (f64.store 1))

  (func (export "test_i32_store8") (param i32 i32) (local.get 0) (local.get 1) (i32.store8 1))
  (func (export "test_i32_store16") (param i32 i32) (local.get 0) (local.get 1) (i32.store16 1))
  (func (export "test_i64_store8") (param i32 i64) (local.get 0) (local.get 1) (i64.store8 1))
  (func (export "test_i64_store16") (param i32 i64) (local.get 0) (local.get 1) (i64.store16 1))
  (func (export "test_i64_store32") (param i32 i64) (local.get 0) (local.get 1) (i64.store32 1))
)
`

// Test:
//
// {i32,i64,f32,f64}.load
// i32.load{8,16}_{s,u}
// i64.load{8,16,32}_{s,u}
//
// {i32,i64,f32,f64}.store
// i32.store{8,16}
// i64.store{8,16,32}

async function test() {
    const mem0 = new WebAssembly.Memory({ initial: 1 });
    const mem1 = new WebAssembly.Memory({ initial: 1 });

    const instance = await instantiate(wat, { js: { memory0: mem0, memory1: mem1 } }, { multi_memory: true });

    const m0array = new Uint8Array(mem0.buffer);
    const m1array = new Uint8Array(mem1.buffer);

    for(let i = 0; i < wasmTestLoopCount; i++) {
        m0array[0] = 0x80;
        m0array[1] = 0x81;
        m0array[2] = 0x82;
        m0array[3] = 0x83;
        m0array[4] = 0x84;
        m0array[5] = 0x85;
        m0array[6] = 0x86;
        m0array[7] = 0x87;

        m1array[0] = 0xC0;
        m1array[1] = 0xC1;
        m1array[2] = 0xC2;
        m1array[3] = 0xC3;
        m1array[4] = 0xC4;
        m1array[5] = 0xC5;
        m1array[6] = 0xC6;
        m1array[7] = 0xC7;

        // test that loads from memory 1 restore default (memory 0) base and size registers
        assert.eq(instance.exports.test_load_m1_then_m0(), 0x40404040);

        assert.eq(instance.exports.i32_load_m1(0), 0xC3C2C1C0 - 0x100000000);
        assert.eq(instance.exports.i64_load_m1(0), 0xC7C6C5C4C3C2C1C0n - (1n << 64n));



        assert.eq(instance.exports.i32_load8s_m1(0), 0xC0 - 0x100);
        assert.eq(instance.exports.i32_load8u_m1(0), 0xC0);
        assert.eq(instance.exports.i32_load16s_m1(0), 0xC1C0 - 0x10000);
        assert.eq(instance.exports.i32_load16u_m1(0), 0xC1C0);



        assert.eq(instance.exports.i64_load8s_m1(0), 0xC0n - 0x100n);
        assert.eq(instance.exports.i64_load8u_m1(0), 0xC0n);
        assert.eq(instance.exports.i64_load16s_m1(0), 0xC1C0n - 0x10000n);
        assert.eq(instance.exports.i64_load16u_m1(0), 0xC1C0n);
        assert.eq(instance.exports.i64_load32s_m1(0), 0xC3C2C1C0n - 0x100000000n);
        assert.eq(instance.exports.i64_load32u_m1(0), 0xC3C2C1C0n);



        instance.exports.test_i32_store(0, 0x3F400000);
        assert.eq(instance.exports.f32_load_m1(0), 0.75);
        instance.exports.test_i64_store(0, 0x3FE8n << 48n);
        assert.eq(instance.exports.f64_load_m1(0), 0.75);

        instance.exports.test_i32_store(0, -1);
        assert.eq(instance.exports.i32_load_m1(0), -1);

        instance.exports.test_i64_store(0, 0x0123456789ABCDEFn);
        assert.eq(instance.exports.i64_load_m1(0), 0x0123456789ABCDEFn);

        instance.exports.test_f32_store(0, 0.75);
        assert.eq(instance.exports.i32_load_m1(0), 0x3F400000);

        instance.exports.test_f64_store(0, 0.75);
        assert.eq(instance.exports.i64_load_m1(0), 0x3FE8n << 48n);



        instance.exports.test_i64_store(0, 0n);

        instance.exports.test_i32_store8(0, -1);
        assert.eq(instance.exports.i64_load_m1(0), 0xFFn);

        instance.exports.test_i32_store16(0, -1);
        assert.eq(instance.exports.i64_load_m1(0), 0xFFFFn);

        instance.exports.test_i64_store(0, 0n);

        instance.exports.test_i64_store8(0, -1n);
        assert.eq(instance.exports.i64_load_m1(0), 0xFFn);

        instance.exports.test_i64_store16(0, -1n);
        assert.eq(instance.exports.i64_load_m1(0), 0xFFFFn);

        instance.exports.test_i64_store32(0, -1n);
        assert.eq(instance.exports.i64_load_m1(0), 0xFFFFFFFFn);
    }
}

await assert.asyncTest(test())
