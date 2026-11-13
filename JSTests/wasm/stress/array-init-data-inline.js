//@ requireOptions("--useConcurrentJIT=0", "--thresholdForOMGOptimizeAfterWarmUp=0", "--thresholdForOMGOptimizeSoon=0")
import { instantiate } from "../gc/wast-wrapper.js"
import * as assert from "../assert.js"

function test() {
    const instance = instantiate(`
      (module
        (type $i8 (array (mut i8)))
        (type $i32 (array (mut i32)))
        (data $d "\\00\\01\\02\\03\\04\\05\\06\\07")
        (func (export "initI8") (param (ref $i8) i32 i32 i32)
          (array.init_data $i8 $d (local.get 0) (local.get 1) (local.get 2) (local.get 3)))
        (func (export "initI32") (param (ref $i32) i32 i32 i32)
          (array.init_data $i32 $d (local.get 0) (local.get 1) (local.get 2) (local.get 3)))
        (func (export "newI8") (param i32) (result (ref $i8))
          (array.new_default $i8 (local.get 0)))
        (func (export "newI32") (param i32) (result (ref $i32))
          (array.new_default $i32 (local.get 0)))
        (func (export "getI8") (param (ref $i8) i32) (result i32)
          (array.get_u $i8 (local.get 0) (local.get 1)))
        (func (export "getI32") (param (ref $i32) i32) (result i32)
          (array.get $i32 (local.get 0) (local.get 1)))
        (func (export "drop")
          (data.drop $d)))
    `);
    const { initI8, initI32, newI8, newI32, getI8, getI32, drop } = instance.exports;

    for (let i = 0; i < wasmTestLoopCount; i++) {
        const bytes = newI8(10);
        initI8(bytes, 0, 0, 8);
        for (let j = 0; j < 8; j++)
            assert.eq(getI8(bytes, j), j);
        assert.eq(getI8(bytes, 8), 0);

        initI8(bytes, 2, 2, 3);
        assert.eq(getI8(bytes, 2), 2);
        assert.eq(getI8(bytes, 3), 3);
        assert.eq(getI8(bytes, 4), 4);

        initI8(bytes, 10, 0, 0);
        initI8(bytes, 0, 8, 0);
        assert.throws(() => initI8(bytes, 11, 0, 0), WebAssembly.RuntimeError, "Out of bounds array.init_data");
        assert.throws(() => initI8(bytes, 9, 0, 2), WebAssembly.RuntimeError, "Out of bounds array.init_data");
        assert.throws(() => initI8(bytes, 0, 8, 1), WebAssembly.RuntimeError, "Out of bounds array.init_data");
        assert.throws(() => initI8(bytes, 0, 0, 9), WebAssembly.RuntimeError, "Out of bounds array.init_data");
        assert.throws(() => initI8(bytes, 0, 0xffffffff, 1), WebAssembly.RuntimeError, "Out of bounds array.init_data");

        const words = newI32(4);
        initI32(words, 1, 0, 2);
        assert.eq(getI32(words, 0), 0);
        assert.eq(getI32(words, 1), 0x03020100);
        assert.eq(getI32(words, 2), 0x07060504);
        assert.eq(getI32(words, 3), 0);
        assert.throws(() => initI32(words, 0, 0, 3), WebAssembly.RuntimeError, "Out of bounds array.init_data");
        assert.throws(() => initI32(words, 0, 1, 2), WebAssembly.RuntimeError, "Out of bounds array.init_data");
    }

    const afterDrop = newI8(4);
    drop();
    initI8(afterDrop, 0, 0, 0);
    assert.throws(() => initI8(afterDrop, 0, 0, 1), WebAssembly.RuntimeError, "Out of bounds array.init_data");
    assert.throws(() => initI8(afterDrop, 0, 1, 0), WebAssembly.RuntimeError, "Out of bounds array.init_data");

    const nullable = instantiate(`
      (module
        (type $i8 (array (mut i8)))
        (data $d "\\00")
        (func (export "init") (param (ref null $i8))
          (array.init_data $i8 $d (local.get 0) (i32.const 0) (i32.const 0) (i32.const 0))))
    `);
    assert.throws(() => nullable.exports.init(null), WebAssembly.RuntimeError, "array.init_data to a null reference");

    const active = instantiate(`
      (module
        (memory 1)
        (type $i8 (array (mut i8)))
        (data (i32.const 0) "xy")
        (func (export "init") (param (ref $i8) i32 i32 i32)
          (array.init_data $i8 0 (local.get 0) (local.get 1) (local.get 2) (local.get 3)))
        (func (export "new") (param i32) (result (ref $i8))
          (array.new_default $i8 (local.get 0))))
    `);
    const arr = active.exports.new(2);
    for (let i = 0; i < wasmTestLoopCount; i++) {
        active.exports.init(arr, 0, 0, 0);
        assert.throws(() => active.exports.init(arr, 0, 0, 1), WebAssembly.RuntimeError, "Out of bounds array.init_data");
    }
}

test();
