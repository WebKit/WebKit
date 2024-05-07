import * as wabt from "../wabt-wrapper.js";
import * as assert from '../assert.js';

let wat = `
(module
    (type (func (param f64) (result i32)))
    (func (type 0) (local f64) (local f64) (local f64) (local i32)
        (local.set 1 (local.get 0))
        (local.set 2 (local.get 0))
        (local.set 3 (local.get 0))
        ;; Do the sub-operation with local/const, const/local, and const/const params
        (local.set 0 (f64.sub (local.get 1) (f64.const nan)))
        (local.set 1 (f64.copysign (local.get 1) (local.get 0)))
        (local.set 0 (f64.sub (f64.const nan) (local.get 2)))
        (local.set 2 (f64.copysign (local.get 2) (local.get 0)))
        (local.set 0 (f64.sub (f64.const 0.0) (f64.const nan)))
        (local.set 3 (f64.copysign (local.get 3) (local.get 0)))
        ;; If any one of the results is negative, then fail: return a positive integer
        ;; Otherwise return 0 for success
        (local.set 4 (f64.lt (local.get 1) (f64.const 0)))
        (local.set 4 (i32.add (local.get 4) (f64.lt (local.get 1) (f64.const 0))))
        (local.set 4 (i32.add (local.get 4) (f64.lt (local.get 2) (f64.const 0))))
        (local.set 4 (i32.add (local.get 4) (f64.lt (local.get 3) (f64.const 0))))
        (local.get 4)
    )
  (export "poc" (func 0))
)
`

async function test() {
    const instance = await wabt.instantiate(wat, {}, {});

    for (let i = 0; i < 50; i++) {
        assert.eq(instance.exports.poc(1), 0);
    }
}

await assert.asyncTest(test());
