import { instantiate } from "../gc/wast-wrapper.js";
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

test();
