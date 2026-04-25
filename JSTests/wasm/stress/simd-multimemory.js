//@ requireOptions("--useWasmMultiMemory=1")

import * as assert from "../assert.js";
import { instantiate } from "../wabt-wrapper.js";

// test atomic instructions that access linear memory

let wat = `
(module
  (import "js" "memory0" (memory 1))
  (import "js" "memory1" (memory 1))

  (func (export "i64_load") (param i32) (result i64) (local.get 0) (i64.load 1))
  (func (export "i64_store") (param i32 i64) (local.get 0) (local.get 1) (i64.store 1))

;; (load address, store address)
  (func (export "test_v128_load") (param i32 i32) (local.get 1) (local.get 0) (v128.load 1) (v128.store 1))

  (func (export "test_v128_load8x8_s") (param i32 i32) (local.get 1) (local.get 0) (v128.load8x8_s 1) (v128.store 1))
  (func (export "test_v128_load8x8_u") (param i32 i32) (local.get 1) (local.get 0) (v128.load8x8_u 1) (v128.store 1))
  (func (export "test_v128_load16x4_s") (param i32 i32) (local.get 1) (local.get 0) (v128.load16x4_s 1) (v128.store 1))
  (func (export "test_v128_load16x4_u") (param i32 i32) (local.get 1) (local.get 0) (v128.load16x4_u 1) (v128.store 1))
  (func (export "test_v128_load32x2_s") (param i32 i32) (local.get 1) (local.get 0) (v128.load32x2_s 1) (v128.store 1))
  (func (export "test_v128_load32x2_u") (param i32 i32) (local.get 1) (local.get 0) (v128.load32x2_u 1) (v128.store 1))

  (func (export "test_v128_load8_splat") (param i32 i32) (local.get 1) (local.get 0) (v128.load8_splat 1) (v128.store 1))
  (func (export "test_v128_load16_splat") (param i32 i32) (local.get 1) (local.get 0) (v128.load16_splat 1) (v128.store 1))
  (func (export "test_v128_load32_splat") (param i32 i32) (local.get 1) (local.get 0) (v128.load32_splat 1) (v128.store 1))
  (func (export "test_v128_load64_splat") (param i32 i32) (local.get 1) (local.get 0) (v128.load64_splat 1) (v128.store 1))

;; lane instructions take arguments (memory index, lane)

;; all lanes are handled using the same IPInt code, testing a sample is enough

  (func (export "test_v128_load8_lane") (param i32 i32) (local.get 1) (local.get 0) (v128.const i64x2 0 0) (v128.load8_lane 1 13) (v128.store 1))

  (func (export "test_v128_load16_lane") (param i32 i32)
    (local.get 1) (local.get 0) (v128.const i64x2 0 0) (v128.load16_lane 1 6) (v128.store 1)
  )

  (func (export "test_v128_load32_lane") (param i32 i32)
    (local.get 1) (local.get 0) (v128.const i64x2 0 0) (v128.load32_lane 1 3) (v128.store 1)
  )

  (func (export "test_v128_load64_lane") (param i32 i32)
    (local.get 1) (local.get 0) (v128.const i64x2 0 0) (v128.load64_lane 1 0) (v128.store 1)
  )

  (func (export "test_v128_load32_zero") (param i32 i32) (local.get 1) (local.get 0) (v128.load32_zero 1) (v128.store 1))
  (func (export "test_v128_load64_zero") (param i32 i32) (local.get 1) (local.get 0) (v128.load64_zero 1) (v128.store 1))


;; v128.store is already executed in load tests

  (func (export "test_v128_store8_lane") (param i32) (local.get 0) (v128.const i64x2 0x7766554433221100 0xFFEEDDCCBBAA9988) (v128.store8_lane 1 8))

  (func (export "test_v128_store16_lane") (param i32) (local.get 0) (v128.const i64x2 0x7766554433221100 0xFFEEDDCCBBAA9988) (v128.store16_lane 1 4))

  (func (export "test_v128_store32_lane") (param i32) (local.get 0) (v128.const i64x2 0x7766554433221100 0xFFEEDDCCBBAA9988) (v128.store32_lane 1 2))

  (func (export "test_v128_store64_lane") (param i32) (local.get 0) (v128.const i64x2 0x7766554433221100 0xFFEEDDCCBBAA9988) (v128.store64_lane 1 1))
)
`;

// Test:
//
// v128.load
// v128.load{8x8,16x4,32x2}_{s,u}
// v128.load{8,16,32,64}_splat
// v128.load{8,16,32,64}_lane
// v128.load{32,64}_zero
// v128.store
// v128.store{8,16,32,64}_lane

async function test() {
    const mem0 = new WebAssembly.Memory({ initial: 1 });
    const mem1 = new WebAssembly.Memory({ initial: 1 });

    const instance = await instantiate(wat, { js: { memory0: mem0, memory1: mem1 } }, { multi_memory: true });

    const m0array = new Uint8Array(mem0.buffer);
    const m1array = new Uint8Array(mem1.buffer);

    for(let i = 0; i < wasmTestLoopCount; i++) {
        instance.exports.i64_store(0, 0x7766554433221100n);
        instance.exports.i64_store(8, 0xFFEEDDCCBBAA9988n);

        instance.exports.test_v128_load(0, 16);
        assert.eq(instance.exports.i64_load(16), 0x7766554433221100n);
        assert.eq(instance.exports.i64_load(24), 0xFFEEDDCCBBAA9988n - (1n << 64n));

        instance.exports.test_v128_load8x8_s(8, 16);
        assert.eq(instance.exports.i64_load(16), 0xFFBBFFAAFF99FF88n - (1n << 64n));
        assert.eq(instance.exports.i64_load(24), 0xFFFFFFEEFFDDFFCCn - (1n << 64n));

        instance.exports.test_v128_load8x8_u(8, 16);
        assert.eq(instance.exports.i64_load(16), 0x00BB00AA00990088n);
        assert.eq(instance.exports.i64_load(24), 0x00FF00EE00DD00CCn);

        instance.exports.test_v128_load16x4_s(8, 16);
        assert.eq(instance.exports.i64_load(16), 0xFFFFBBAAFFFF9988n - (1n << 64n));
        assert.eq(instance.exports.i64_load(24), 0xFFFFFFEEFFFFDDCCn - (1n << 64n));

        instance.exports.test_v128_load16x4_u(8, 16);
        assert.eq(instance.exports.i64_load(16), 0x0000BBAA00009988n);
        assert.eq(instance.exports.i64_load(24), 0x0000FFEE0000DDCCn);

        instance.exports.test_v128_load32x2_s(8, 16);
        assert.eq(instance.exports.i64_load(16), 0xFFFFFFFFBBAA9988n - (1n << 64n));
        assert.eq(instance.exports.i64_load(24), 0xFFFFFFFFFFEEDDCCn - (1n << 64n));

        instance.exports.test_v128_load32x2_u(8, 16);
        assert.eq(instance.exports.i64_load(16), 0x00000000BBAA9988n);
        assert.eq(instance.exports.i64_load(24), 0x00000000FFEEDDCCn);

        instance.exports.test_v128_load8_splat(1, 16);
        assert.eq(instance.exports.i64_load(16), 0x1111111111111111n);
        assert.eq(instance.exports.i64_load(24), 0x1111111111111111n);

        instance.exports.test_v128_load16_splat(0, 16);
        assert.eq(instance.exports.i64_load(16), 0x1100110011001100n);
        assert.eq(instance.exports.i64_load(24), 0x1100110011001100n);

        instance.exports.test_v128_load32_splat(0, 16);
        assert.eq(instance.exports.i64_load(16), 0x3322110033221100n);
        assert.eq(instance.exports.i64_load(24), 0x3322110033221100n);

        instance.exports.test_v128_load64_splat(0, 16);
        assert.eq(instance.exports.i64_load(16), 0x7766554433221100n);
        assert.eq(instance.exports.i64_load(24), 0x7766554433221100n);

        instance.exports.test_v128_load8_lane(8, 16);
        assert.eq(instance.exports.i64_load(16), 0n);
        assert.eq(instance.exports.i64_load(24), 0x0000880000000000n);

        instance.exports.test_v128_load16_lane(8, 16);
        assert.eq(instance.exports.i64_load(16), 0n);
        assert.eq(instance.exports.i64_load(24), 0x0000998800000000n);

        instance.exports.test_v128_load32_lane(8, 16);
        assert.eq(instance.exports.i64_load(16), 0n);
        assert.eq(instance.exports.i64_load(24), 0xBBAA998800000000n - (1n << 64n));

        instance.exports.test_v128_load64_lane(0, 16);
        assert.eq(instance.exports.i64_load(16), 0x7766554433221100n);
        assert.eq(instance.exports.i64_load(24), 0n);

        instance.exports.test_v128_load32_zero(0, 16);
        assert.eq(instance.exports.i64_load(16), 0x33221100n);
        assert.eq(instance.exports.i64_load(24), 0n);

        instance.exports.test_v128_load64_zero(0, 16);
        assert.eq(instance.exports.i64_load(16), 0x7766554433221100n);
        assert.eq(instance.exports.i64_load(24), 0n);

        instance.exports.i64_store(0, 0n);
        instance.exports.test_v128_store8_lane(0);
        assert.eq(instance.exports.i64_load(0), 0x88n);

        instance.exports.i64_store(0, 0n);
        instance.exports.test_v128_store16_lane(0);
        assert.eq(instance.exports.i64_load(0), 0x9988n);

        instance.exports.i64_store(0, 0n);
        instance.exports.test_v128_store32_lane(0);
        assert.eq(instance.exports.i64_load(0), 0xBBAA9988n);

        instance.exports.i64_store(0, 0n);
        instance.exports.test_v128_store64_lane(0);
        assert.eq(instance.exports.i64_load(0), 0xFFEEDDCCBBAA9988n - (1n << 64n));
   }
}

await assert.asyncTest(test());
