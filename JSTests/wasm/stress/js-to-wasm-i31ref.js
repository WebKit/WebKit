import { instantiate } from "../gc/wast-wrapper.js";
import * as assert from "../assert.js";

let wat = `
(module
  (func (export "get") (param (ref i31)) (result i32)
    (i31.get_s (local.get 0)))
  (func (export "getNullable") (param i31ref) (result i32)
    (if (result i32)
      (ref.is_null (local.get 0))
      (then (i32.const -1))
      (else (i31.get_s (local.get 0)))))
)
`;

async function test() {
    const instance = instantiate(wat);
    const { get, getNullable } = instance.exports;

    for (let i = 0; i < wasmTestLoopCount; i++) {
        assert.eq(get(0), 0);
        assert.eq(get(2), 2);
        assert.eq(get(2 ** 30 - 1), 2 ** 30 - 1);
        assert.eq(get(-(2 ** 30)), -(2 ** 30));
        assert.eq(getNullable(null), -1);
        assert.eq(getNullable(7), 7);
    }

    assert.throws(() => get(2.3), TypeError, "Argument value did not match the reference type");
    assert.throws(() => get(2n), TypeError, "Argument value did not match the reference type");
    assert.throws(() => get(2 ** 30), TypeError, "Argument value did not match the reference type");
    assert.throws(() => get(-(2 ** 30) - 1), TypeError, "Argument value did not match the reference type");
    assert.throws(() => get(null), TypeError, "Argument value did not match the reference type");
}

await assert.asyncTest(test());
