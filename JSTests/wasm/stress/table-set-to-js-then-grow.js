import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (type $ri (func (result i32)))
    (import "e" "foo" (func $foo (result i32)))
    (export "foo" (func $foo))
    (table $table 0 funcref)
    (func (export "test") (param funcref) (result i32)
        (table.grow $table (ref.null func) (i32.const 1))
        drop
        (table.set $table (i32.const 0) (local.get 0))
        (table.grow $table (ref.null func) (i32.const 10))
        drop
        (call_indirect $table (type $ri) (i32.const 0))
    )
)
`

function foo() { return 0; }
const instance = await instantiate(wat, { e: { foo } });

for (let i = 0; i < 1e3; ++i)
    assert.eq(instance.exports.test(instance.exports.foo), 0);
