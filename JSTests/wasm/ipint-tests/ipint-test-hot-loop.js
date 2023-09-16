import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (func (export "isPrime") (param i32) (result i32)
        (local i32 i32)
        (local.get 0)
        (i32.const 2)
        (local.tee 1)
        (i32.lt_s)
        (if
            (then
                (i32.const 0)
                (return)
            )
        )
        (loop
            (local.get 0)
            (local.get 1)
            (i32.rem_s)
            (if
                (then
                    (local.get 1)
                    (i32.const 1)
                    (i32.add)
                    (local.set 1)
                )
                (else
                    (i32.const 0)
                    (return)
                )
            )
            (local.get 1)
            (local.get 0)
            (i32.lt_s)
            (br_if 0)
        )
        (local.get 0)
        (local.get 1)
        (i32.eq)
        (return)
    )
)
`

async function test() {
    const instance = await instantiate(wat, {});
    const { isPrime } = instance.exports
    assert.eq(isPrime(101), 1);
    assert.eq(isPrime(65537), 1);
    assert.eq(isPrime(1048577), 0);
}

assert.asyncTest(test())
