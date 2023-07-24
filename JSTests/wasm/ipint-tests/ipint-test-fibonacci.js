import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (func (export "fib") (param i32) (result i32)
        (local i32 i32)
        (i32.const 1)
        (local.set 1)
        (loop
            (local.get 2)
            (local.get 1)
            (local.get 2)
            (i32.add)
            (local.set 2)
            (local.set 1)
            (local.get 0)
            (i32.const 1)
            (i32.sub)
            (local.tee 0)
            (i32.const 0)
            (i32.gt_s)
            (br_if 0)
        )
        (local.get 2)
        (return)
    )
)
`

async function test() {
    const instance = await instantiate(wat, {});
    const { fib } = instance.exports
    let a = 0;
    let b = 1;
    for (let i = 1; i < 47; ++i) {
        assert.eq(fib(i), b);
        [a, b] = [b, a+b];
    }
}

assert.asyncTest(test())
