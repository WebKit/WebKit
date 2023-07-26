import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (global $value (export "value") (mut i32) (i32.const 5))
    (func (export "set")
        (i32.const 10)
        (global.set $value)
    )
    (func (export "read") (result i32)
        (global.get $value)
    )
)
`

async function test() {
    const instance = await instantiate(wat, {});
    const { value, set, read } = instance.exports
    assert.eq(read(), 5);
    set();
    assert.eq(value.value, 10);
}

assert.asyncTest(test())
