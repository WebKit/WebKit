import { compile, instantiate } from "../gc/wast-wrapper.js";
import * as assert from "../assert.js";

function test() {
    const instance = instantiate(`
      (module
        (func (export "toI31") (param externref) (result i32)
          (i31.get_s (ref.cast (ref i31) (any.convert_extern (local.get 0)))))
        (func (export "pass") (param externref) (result anyref)
          (any.convert_extern (local.get 0)))
      )
    `);
    const { toI31, pass } = instance.exports;

    assert.eq(toI31(0), 0);
    assert.eq(toI31(5), 5);
    assert.eq(toI31(5.0), 5);
    assert.eq(toI31(2 ** 30 - 1), 2 ** 30 - 1);
    assert.eq(toI31(-(2 ** 30)), -(2 ** 30));

    const object = { };
    assert.eq(pass(object), object);
    assert.eq(pass(null), null);

    for (let i = 0; i < wasmTestLoopCount; i++) {
        assert.eq(toI31(i & 0xff), i & 0xff);
        assert.eq(pass(object), object);
        assert.eq(pass(null), null);
    }

    assert.throws(() => toI31(2 ** 30), WebAssembly.RuntimeError, "ref.cast failed to cast reference to target heap type");
    assert.throws(() => toI31(5.3), WebAssembly.RuntimeError, "ref.cast failed to cast reference to target heap type");
    assert.throws(() => toI31("foo"), WebAssembly.RuntimeError, "ref.cast failed to cast reference to target heap type");
}

function testNoextern() {
    const instance = instantiate(`
      (module
        (global $g anyref (any.convert_extern (ref.null noextern)))
        (func (export "constant") (result anyref)
          (any.convert_extern (ref.null noextern)))
        (func (export "local") (result anyref) (local (ref null noextern))
          (any.convert_extern (local.get 0)))
        (func (export "nonNull") (result (ref any))
          (any.convert_extern (ref.as_non_null (ref.null noextern))))
        (func (export "global") (result anyref)
          (global.get $g))
      )
    `);
    const { constant, local, nonNull, global } = instance.exports;

    for (let i = 0; i < wasmTestLoopCount; i++) {
        assert.eq(constant(), null);
        assert.eq(local(), null);
        assert.eq(global(), null);
    }
    assert.throws(() => nonNull(), WebAssembly.RuntimeError, "ref.as_non_null to a null reference");
}

function testInvalid() {
    for (const heapType of ["nofunc", "func", "none", "any"]) {
        assert.throws(() => compile(`
          (module
            (func (result anyref)
              (any.convert_extern (ref.null ${heapType}))))
        `), WebAssembly.CompileError, `any.convert_extern reference to type (ref null ${heapType}) expected (ref null extern)`);
    }

    assert.throws(() => compile(`
      (module
        (func (result (ref any))
          (any.convert_extern (ref.null noextern))))
    `), WebAssembly.CompileError, "control flow returns with unexpected type. (ref null any) is not a (ref any)");

    assert.throws(() => compile(`
      (module
        (global (ref any) (any.convert_extern (ref.null noextern))))
    `), WebAssembly.CompileError, "control flow returns with unexpected type. (ref null any) is not a (ref any)");
}

test();
testNoextern();
testInvalid();
