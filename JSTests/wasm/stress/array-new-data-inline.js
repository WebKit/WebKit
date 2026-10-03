//@ requireOptions("--useConcurrentJIT=0", "--thresholdForOMGOptimizeAfterWarmUp=0", "--thresholdForOMGOptimizeSoon=0")
//@ $skipModes << "wasm-no-jit".to_sym
//@ $skipModes << "wasm-no-wasm-jit".to_sym
import { instantiate } from "../gc/wast-wrapper.js"
import * as assert from "../assert.js"

function test() {
    const instance = instantiate(`
      (module
        (type $i8 (array (mut i8)))
        (type $i32 (array (mut i32)))
        (data $d "\\00\\01\\02\\03\\04\\05\\06\\07")
        (func (export "newI8") (param i32 i32) (result (ref $i8))
          (array.new_data $i8 $d (local.get 0) (local.get 1)))
        (func (export "newI32") (param i32 i32) (result (ref $i32))
          (array.new_data $i32 $d (local.get 0) (local.get 1)))
        (func (export "getI8") (param (ref $i8) i32) (result i32)
          (array.get_u $i8 (local.get 0) (local.get 1)))
        (func (export "getI32") (param (ref $i32) i32) (result i32)
          (array.get $i32 (local.get 0) (local.get 1)))
        (func (export "drop")
          (data.drop $d)))
    `);
    const { newI8, newI32, getI8, getI32, drop } = instance.exports;

    for (let i = 0; i < wasmTestLoopCount; i++) {
        const bytes = newI8(8, 0);
        for (let j = 0; j < 8; j++)
            assert.eq(getI8(bytes, j), j);

        const slice = newI8(3, 2);
        assert.eq(getI8(slice, 0), 2);
        assert.eq(getI8(slice, 1), 3);
        assert.eq(getI8(slice, 2), 4);

        newI8(0, 0);
        newI8(0, 8);

        assert.throws(() => newI8(1, 8), WebAssembly.RuntimeError, "Out of bounds or failed to allocate in array.new_data");
        assert.throws(() => newI8(9, 0), WebAssembly.RuntimeError, "Out of bounds or failed to allocate in array.new_data");
        assert.throws(() => newI8(1, 0xffffffff), WebAssembly.RuntimeError, "Out of bounds or failed to allocate in array.new_data");

        const words = newI32(2, 0);
        assert.eq(getI32(words, 0), 0x03020100);
        assert.eq(getI32(words, 1), 0x07060504);
        assert.throws(() => newI32(3, 0), WebAssembly.RuntimeError, "Out of bounds or failed to allocate in array.new_data");
        assert.throws(() => newI32(2, 1), WebAssembly.RuntimeError, "Out of bounds or failed to allocate in array.new_data");
    }

    drop();
    newI8(0, 0);
    assert.throws(() => newI8(0, 1), WebAssembly.RuntimeError, "Out of bounds or failed to allocate in array.new_data");
    assert.throws(() => newI8(1, 0), WebAssembly.RuntimeError, "Out of bounds or failed to allocate in array.new_data");

    const active = instantiate(`
      (module
        (memory 1)
        (type $i8 (array (mut i8)))
        (data (i32.const 0) "xy")
        (func (export "new") (param i32 i32) (result (ref $i8))
          (array.new_data $i8 0 (local.get 0) (local.get 1))))
    `);
    for (let i = 0; i < wasmTestLoopCount; i++) {
        active.exports.new(0, 0);
        assert.throws(() => active.exports.new(1, 0), WebAssembly.RuntimeError, "Out of bounds or failed to allocate in array.new_data");
    }
}

test();
