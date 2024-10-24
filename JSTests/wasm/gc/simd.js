//@ skip unless $isSIMDPlatform
//@ requireOptions("--useWasmSIMD=1")
//@ runWebAssemblySuite("--useWasmGC=true")

import * as assert from "../assert.js";
import { compile, instantiate } from "./wast-wrapper.js";

function testSIMDStruct() {
  instantiate(`
    (module
      (type (struct (field v128))))
  `);

  instantiate(`
    (module
      (type (struct (field (mut v128)))))
  `);

  {
    const m = instantiate(`
      (module
        (type (struct (field (mut v128))))
        (global (ref 0) (struct.new 0 (v128.const i64x2 0xFFFFFFFFFFFFFFFF 0xFFFFFFFFFFFFFFFF)))
        (func (result v128)
          (struct.get 0 0 (global.get 0)))
        (func (export "f") (result i32)
          (i64x2.all_true
            (i64x2.eq (call 0) (v128.const i64x2 0xFFFFFFFFFFFFFFFF 0xFFFFFFFFFFFFFFFF))))
      )
    `);
    assert.eq(m.exports.f(), 1);
  }

  {
    const m = instantiate(`
      (module
        (type (struct (field (mut v128))))
        (global (mut (ref null 0)) (ref.null 0))
        (start 0)
        (func
          (global.set 0 (struct.new 0 (v128.const i64x2 0x42 0x84))))
        (func (export "lane0") (result i64)
          (i64x2.extract_lane 0 (struct.get 0 0 (global.get 0))))
        (func (export "lane1") (result i64)
          (i64x2.extract_lane 1 (struct.get 0 0 (global.get 0))))
      )
    `);
    assert.eq(m.exports.lane0(), 0x42n);
    assert.eq(m.exports.lane1(), 0x84n);
  }

  {
    const m = instantiate(`
      (module
        (type (struct (field (mut v128))))
        (global (mut (ref null 0)) (ref.null 0))
        (start 0)
        (func
          (global.set 0 (struct.new_default 0)))
        (func (export "lane0") (result i64)
          (i64x2.extract_lane 0 (struct.get 0 0 (global.get 0))))
        (func (export "lane1") (result i64)
          (i64x2.extract_lane 1 (struct.get 0 0 (global.get 0))))
      )
    `);
    assert.eq(m.exports.lane0(), 0n);
    assert.eq(m.exports.lane1(), 0n);
  }

  {
    const m = instantiate(`
      (module
        (type (struct (field i32 i32 i8 v128 i16)))
        (global (mut (ref null 0)) (ref.null 0))
        (start 0)
        (func
          (global.set 0 (struct.new 0 (i32.const 31) (i32.const 63) (i32.const 127) (v128.const i64x2 0x42 0x84) (i32.const 255))))
        (func (export "get0") (result i32) (struct.get 0 0 (global.get 0)))
        (func (export "get1") (result i32) (struct.get 0 1 (global.get 0)))
        (func (export "get2") (result i32) (struct.get_u 0 2 (global.get 0)))
        (func (export "get4") (result i32) (struct.get_u 0 4 (global.get 0)))
        (func (export "lane0") (result i64)
          (i64x2.extract_lane 0 (struct.get 0 3 (global.get 0))))
        (func (export "lane1") (result i64)
          (i64x2.extract_lane 1 (struct.get 0 3 (global.get 0))))
      )
    `);
    assert.eq(m.exports.lane0(), 0x42n);
    assert.eq(m.exports.lane1(), 0x84n);
    assert.eq(m.exports.get0(), 31);
    assert.eq(m.exports.get1(), 63);
    assert.eq(m.exports.get2(), 127);
    assert.eq(m.exports.get4(), 255);
  }

  {
    const m = instantiate(`
      (module
        (type (struct (field (mut v128))))
        (global (ref 0) (struct.new 0 (v128.const i64x2 0xFFFFFFFFFFFFFFFF 0xFFFFFFFFFFFFFFFF)))
        (func (result v128)
          (struct.set 0 0 (global.get 0) (v128.const i64x2 0x01 0x2))
          (struct.get 0 0 (global.get 0)))
        (func (export "f") (result i32)
          (i64x2.all_true
            (i64x2.eq (call 0) (v128.const i64x2 0x1 0x2))))
      )
    `);
    assert.eq(m.exports.f(), 1);
  }
}

function testSIMDArray() {
  instantiate(`
    (module
      (type (array v128)))
  `);

  instantiate(`
    (module
      (type (array (mut v128))))
  `);

  {
    const m = instantiate(`
      (module
        (type (array v128))
        (global (mut (ref null 0)) (ref.null 0))
        (start 0)
        (func
          (global.set 0 (array.new 0 (v128.const i64x2 0x01 0x02) (i32.const 5))))
        (func (export "lane0") (param i32) (result i64)
          (i64x2.extract_lane 0 (array.get 0 (global.get 0) (local.get 0))))
        (func (export "lane1") (param i32) (result i64)
          (i64x2.extract_lane 1 (array.get 0 (global.get 0) (local.get 0))))
      )
    `);
    for (var i = 0; i < 5; i++) {
      assert.eq(m.exports.lane0(i), 1n);
      assert.eq(m.exports.lane1(i), 2n);
    }
  }

  {
    const m = instantiate(`
      (module
        (type (array v128))
        (global (mut (ref null 0)) (ref.null 0))
        (start 0)
        (func
          (global.set 0 (array.new_default 0 (i32.const 5))))
        (func (export "lane0") (param i32) (result i64)
          (i64x2.extract_lane 0 (array.get 0 (global.get 0) (local.get 0))))
        (func (export "lane1") (param i32) (result i64)
          (i64x2.extract_lane 1 (array.get 0 (global.get 0) (local.get 0))))
      )
    `);
    for (var i = 0; i < 5; i++) {
      assert.eq(m.exports.lane0(i), 0n);
      assert.eq(m.exports.lane1(i), 0n);
    }
  }

  {
    const m = instantiate(`
      (module
        (type (array v128))
        (global (mut (ref null 0)) (ref.null 0))
        (start 0)
        (func
          (global.set 0 (array.new_fixed 0 3 (v128.const i64x2 0x01 0x02) (v128.const i64x2 0x3 0x4) (v128.const i64x2 0x5 0x6))))
        (func (export "lane0") (param i32) (result i64)
          (i64x2.extract_lane 0 (array.get 0 (global.get 0) (local.get 0))))
        (func (export "lane1") (param i32) (result i64)
          (i64x2.extract_lane 1 (array.get 0 (global.get 0) (local.get 0))))
      )
    `);
    assert.eq(m.exports.lane0(0), 1n);
    assert.eq(m.exports.lane1(0), 2n);
    assert.eq(m.exports.lane0(1), 3n);
    assert.eq(m.exports.lane1(1), 4n);
    assert.eq(m.exports.lane0(2), 5n);
    assert.eq(m.exports.lane1(2), 6n);
  }

  {
    const m = instantiate(`
      (module
        (type (array v128))
        (global (ref 0) (array.new 0 (v128.const i64x2 0xFFFFFFFFFFFFFFFF 0xFFFFFFFFFFFFFFFF) (i32.const 5)))
        (func (param i32) (result v128)
          (array.get 0 (global.get 0) (local.get 0)))
        (func (export "f") (param i32) (result i32)
          (i64x2.all_true
            (i64x2.eq (call 0 (local.get 0)) (v128.const i64x2 0xFFFFFFFFFFFFFFFF 0xFFFFFFFFFFFFFFFF))))
      )
    `);
    for (var i = 0; i < 5; i++)
      assert.eq(m.exports.f(i), 1);
  }

  {
    const m = instantiate(`
      (module
        (type (array v128))
        (global (ref 0) (array.new_fixed 0 2 (v128.const i64x2 0x1 0x2) (v128.const i64x2 0x3 0x4)))
        (func (export "lane0") (param i32) (result i64)
          (i64x2.extract_lane 0
            (array.get 0 (global.get 0) (local.get 0))))
        (func (export "lane1") (param i32) (result i64)
          (i64x2.extract_lane 1
            (array.get 0 (global.get 0) (local.get 0))))
      )
    `);
    assert.eq(m.exports.lane0(0), 0x1n);
    assert.eq(m.exports.lane1(0), 0x2n);
    assert.eq(m.exports.lane0(1), 0x3n);
    assert.eq(m.exports.lane1(1), 0x4n);
  }

  {
    const m = instantiate(`
      (module
        (type (array (mut v128)))
        (global (ref 0) (array.new 0 (v128.const i64x2 0xFFFFFFFFFFFFFFFF 0xFFFFFFFFFFFFFFFF) (i32.const 5)))
        (start 0)
        (func
          (array.set 0 (global.get 0) (i32.const 2) (v128.const i64x2 0x1 0x2)))
        (func (param i32) (result v128)
          (array.get 0 (global.get 0) (local.get 0)))
        (func (export "f") (param i32) (result i32)
          (i64x2.all_true
            (i64x2.eq (call 1 (local.get 0)) (v128.const i64x2 0xFFFFFFFFFFFFFFFF 0xFFFFFFFFFFFFFFFF))))
        (func (export "g") (param i32) (result i32)
          (i64x2.all_true
            (i64x2.eq (call 1 (local.get 0)) (v128.const i64x2 0x1 0x2))))
      )
    `);
    for (var i = 0; i < 2; i++)
      assert.eq(m.exports.f(i), 1);
    assert.eq(m.exports.g(2), 1);
    for (var i = 3; i < 5; i++)
      assert.eq(m.exports.f(i), 1);
  }

  {
    const m = instantiate(`
      (module
        (type (array (mut v128)))
        (global (ref 0) (array.new 0 (v128.const i64x2 0xFFFFFFFFFFFFFFFF 0xFFFFFFFFFFFFFFFF) (i32.const 5)))
        (global i32 (i32.const 2))
        (start 0)
        (func
          (array.set 0 (global.get 0) (global.get 1) (v128.const i64x2 0x1 0x2)))
        (func (param i32) (result v128)
          (array.get 0 (global.get 0) (local.get 0)))
        (func (export "f") (param i32) (result i32)
          (i64x2.all_true
            (i64x2.eq (call 1 (local.get 0)) (v128.const i64x2 0xFFFFFFFFFFFFFFFF 0xFFFFFFFFFFFFFFFF))))
        (func (export "g") (param i32) (result i32)
          (i64x2.all_true
            (i64x2.eq (call 1 (local.get 0)) (v128.const i64x2 0x1 0x2))))
      )
    `);
    for (var i = 0; i < 2; i++)
      assert.eq(m.exports.f(i), 1);
    assert.eq(m.exports.g(2), 1);
    for (var i = 3; i < 5; i++)
      assert.eq(m.exports.f(i), 1);
  }

  {
    const m = instantiate(`
      (module
        (type (array v128))
        (data "\\00\\01\\02\\03\\04\\05\\06\\07\\08\\09\\0A\\0B\\0C\\0D\\0E\\0F\\10\\11\\12\\13\\14\\15\\16\\17\\18\\19\\1A\\1B\\1C\\1D\\1E\\1F")
        (global (mut (ref null 0)) (ref.null 0))
        (start 0)
        (func
          (global.set 0 (array.new_data 0 0 (i32.const 0) (i32.const 2))))
        (func (param i32) (result v128)
          (array.get 0 (global.get 0) (local.get 0)))
        (func (export "lane0") (param i32) (result i64)
          (i64x2.extract_lane 0 (call 1 (local.get 0))))
        (func (export "lane1") (param i32) (result i64)
          (i64x2.extract_lane 1 (call 1 (local.get 0))))
      )
    `);
    assert.eq(m.exports.lane0(0), 0x0706050403020100n);
    assert.eq(m.exports.lane1(0), 0x0F0E0D0C0B0A0908n);
    assert.eq(m.exports.lane0(1), 0x1716151413121110n);
    assert.eq(m.exports.lane1(1), 0x1F1E1D1C1B1A1918n);
  }

  {
    const m = instantiate(`
      (module
        (type (array (mut v128)))
        (data "\\00\\01\\02\\03\\04\\05\\06\\07\\08\\09\\0A\\0B\\0C\\0D\\0E\\0F\\10\\11\\12\\13\\14\\15\\16\\17\\18\\19\\1A\\1B\\1C\\1D\\1E\\1F")
        (global (ref null 0) (array.new_default 0 (i32.const 5)))
        (start 0)
        (func
          (array.init_data 0 0 (global.get 0) (i32.const 0) (i32.const 0) (i32.const 2)))
        (func (param i32) (result v128)
          (array.get 0 (global.get 0) (local.get 0)))
        (func (export "lane0") (param i32) (result i64)
          (i64x2.extract_lane 0 (call 1 (local.get 0))))
        (func (export "lane1") (param i32) (result i64)
          (i64x2.extract_lane 1 (call 1 (local.get 0))))
      )
    `);

    assert.eq(m.exports.lane0(0), 0x0706050403020100n);
    assert.eq(m.exports.lane1(0), 0x0F0E0D0C0B0A0908n);
    assert.eq(m.exports.lane0(1), 0x1716151413121110n);
    assert.eq(m.exports.lane1(1), 0x1F1E1D1C1B1A1918n);
    for (var i = 2; i < 5; i++) {
      assert.eq(m.exports.lane0(i), 0n);
      assert.eq(m.exports.lane1(i), 0n);
    }
  }

  {
     const m = instantiate(`
       (module
         (type (array (mut v128)))
         (global $fill v128 (v128.const i64x2 0x42 0x84))
         (global $arr (ref 0) (array.new_default 0 (i32.const 10)))
         (func
           (array.fill 0 (global.get $arr) (i32.const 1) (global.get $fill) (i32.const 5)))
         (func (export "lane0") (param i32) (result i64)
           (i64x2.extract_lane 0
             (array.get 0 (global.get $arr) (local.get 0))))
         (func (export "lane1") (param i32) (result i64)
           (i64x2.extract_lane 1
             (array.get 0 (global.get $arr) (local.get 0))))
         (start 0))
    `);

    assert.eq(m.exports.lane0(0), 0n);
    assert.eq(m.exports.lane1(0), 0n);
    for (let i = 1; i < 6; i++) {
      assert.eq(m.exports.lane0(i), 0x42n);
      assert.eq(m.exports.lane1(i), 0x84n);
    }
    for (let i = 6; i < 10; i++) {
      assert.eq(m.exports.lane0(i), 0n);
      assert.eq(m.exports.lane1(i), 0n);
    }
  }

  {
    const m = instantiate(`
      (module
        (type (array (mut v128)))
        (global $fill v128 (v128.const i64x2 0x42 0x84))
        (global $dst (ref 0) (array.new_default 0 (i32.const 10)))
        (global $src (ref 0) (array.new_default 0 (i32.const 10)))
        (func
          (array.fill 0 (global.get $src) (i32.const 3) (global.get $fill) (i32.const 4))
          (array.copy 0 0 (global.get $dst) (i32.const 1) (global.get $src) (i32.const 2) (i32.const 6)))
        (func (export "lane0") (param i32) (result i64)
          (i64x2.extract_lane 0
            (array.get 0 (global.get $dst) (local.get 0))))
        (func (export "lane1") (param i32) (result i64)
          (i64x2.extract_lane 1
            (array.get 0 (global.get $dst) (local.get 0))))
        (start 0))
    `);

    for (let i = 0; i < 2; i++) {
      assert.eq(m.exports.lane0(i), 0n);
      assert.eq(m.exports.lane1(i), 0n);
    }
    for (let i = 2; i < 6; i++) {
      assert.eq(m.exports.lane0(i), 0x42n);
      assert.eq(m.exports.lane1(i), 0x84n);
    }
    for (let i = 6; i < 10; i++) {
      assert.eq(m.exports.lane0(i), 0n);
      assert.eq(m.exports.lane1(i), 0n);
    }
  }
}

function testJSAPI() {
  {
    const m = instantiate(`
      (module
        (type (array v128))
        (global (export "arr") (mut (ref null 0)) (ref.null 0))
        (start 0)
        (func
          (global.set 0 (array.new_default 0 (i32.const 5))))
        (func (export "lane0") (param (ref 0) i32) (result i64)
          (i64x2.extract_lane 0 (array.get 0 (local.get 0) (local.get 1))))
        (func (export "lane1") (param (ref 0) i32) (result i64)
          (i64x2.extract_lane 1 (array.get 0 (local.get 0) (local.get 1))))
      )
    `);
    for (var i = 0; i < 5; i++) {
      assert.eq(m.exports.lane0(m.exports.arr.value, i), 0n);
      assert.eq(m.exports.lane1(m.exports.arr.value, i), 0n);
    }
  }
}

function testSIMDGlobal() {
  instantiate(`
    (module
      (global v128 (v128.const i64x2 0 0))
      (global v128 (v128.const i64x2 1 1))
      (global v128 (v128.const i64x2 2 2))
      (global v128 (global.get 1))
      (global v128 (global.get 3)))
  `);
}

testSIMDStruct();
testSIMDArray();
testJSAPI();
testSIMDGlobal();
