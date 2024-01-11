//@ skip if !$isSIMDPlatform
import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"


let verbose = false;

let wat = `
(module
    (func $log_value (import "a" "log_value") (param i32))

    (func (export "test") (param $p0 i32) (param $p1 i32) (param $p2 i32)
        (local.get $p1)
        (local.get $p1)
        (local.get $p1)
        (local.get $p1)
        (local.get $p1)
        (local.get $p1)
        (local.get $p1)
        (local.get $p1)
        (local.get $p1)

        (if (result i32)
        (local.get $p0)
        (then
            (local.set $p2
                (local.get $p0))
            (i32.const 0))
        (else
            (i32.const 0)))

        (local.get $p2)
        (call $f)

    )

    (func $f (param i32) (param i32) (param i32) (param i32) (param i32) (param i32) (param i32) (param i32) (param i32) (param i32) (param $pl i32)
        (call $log_value
            (local.get $pl))
    )
)
`

async function test() {
    let log = []
    const instance = await instantiate(wat, { a: {
        log_value: function(i) { log.push("value: " + i); },
    } }, { simd: true })
    const { test, test2 } = instance.exports

    let lastLog = []
    for (let i = 0; i < 10000; ++i) {
        assert.eq(test(0, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21), undefined)

        if (i > 0 && JSON.stringify(lastLog) != JSON.stringify(log)) {
            print(lastLog)
            print(log)
            throw ""
        }

        lastLog = log
        log = []
    }

    if (verbose) print(lastLog)
    if (lastLog != "value: 3")
        throw ""

    if (verbose) print("Success")
}

assert.asyncTest(test())
