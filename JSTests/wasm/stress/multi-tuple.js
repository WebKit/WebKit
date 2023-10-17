import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

const N = 1000

let wat = `
(module
  (type (;0;) (func (result i32 i64)))
  (type (;1;) (func))
  (type (;2;) (func (result i32 i32)))
  (import "m" "fn0" (func (;0;) (type 2)))
  (func (;1;) (type 0) (result i32 i64)
    i32.const 0
    i64.const 0
  )
  (func (;2;) (type 1)
    call 1
    call 0
    return
  )
  (export "fn1" (func 2))
)
`

async function test() {
    const instance = await instantiate(wat, {
        m: {
            fn0() {
                return [0, 0];
            }
        }
    });
    const { fn1 } = instance.exports

    for (let i = 0; i < 10000; ++i) {
        assert.eq(fn1(), undefined);
    }
}

assert.asyncTest(test())
