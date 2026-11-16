import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (func $fac (export "fac") (param $x i64) (result i64)
      (return_call $fac_aux (local.get $x) (i64.const 1))
    )

    (func $fac_aux (param $x i64) (param $r i64) (result i64)
      (if (result i64) (i64.eqz (local.get $x))
        (then
          (return (local.get $r))
        )
        (else
          (i64.sub (local.get $x) (i64.const 1))
          (i64.mul (local.get $x) (local.get $r))
          (return_call $fac_aux)
        )
      )
    )
)
`

async function test() {
    const instance = await instantiate(wat, {}, { tail_call: true });
    const { fac } = instance.exports
    assert.eq(fac(0n), 1n);
    assert.eq(fac(1n), 1n);
    assert.eq(fac(2n), 2n);
    assert.eq(fac(3n), 6n);
    assert.eq(fac(4n), 24n);
    assert.eq(fac(5n), 120n);
    assert.eq(fac(6n), 720n);
    assert.eq(fac(7n), 5040n);
}

await assert.asyncTest(test())
