//@ requireOptions("--useWasmJITLessJSEntrypoint=1")
import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let type = 'i32'

let wat = `
(module
    (type $sig_test (func (param ${type}) (result ${type})))
    (import "o" "callee" (func $callee (param $x ${type}) (result ${type})))
    (func $test (export "test") (param $x ${type}) (result ${type})
        (local.get $x)
        (call $callee)
    )
)
`

function callee(x) {
    return x + 1;
}

async function test() {
    const instance = await instantiate(wat, { o: { callee } })
    const { test } = instance.exports

    assert.eq(test(5), 6)
}

await assert.asyncTest(test())
