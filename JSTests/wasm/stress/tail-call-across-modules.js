//@ requireOptions("--useWasmTailCalls=true", "--maximumWasmCalleeSizeForInlining=0")
import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat1 = `
(module
    (table 1 funcref)
    (table 2 funcref)
    (table 3 funcref)
    (table 4 funcref)

    (func (export "cross_module_callee")
        (table.size 2)
        (drop)
        (return)
    )
)
`

let wat2 = `
(module
    (import "o" "cross_module_callee" (func $cross_module_callee))

    (table 10 funcref)
    (table 11 funcref)
    (table 12 funcref)
    (table 13 funcref)
    (table 14 funcref)
    (table 15 funcref)
    (table 16 funcref)
    (table 17 funcref)
    (table 18 funcref)
    (table 19 funcref)

    (func $local_callee
        (table.size 6)
        (drop)
        (return)
    )

    (func $foo (export "foo") (param $x i32)
        (table.size 7)
        (drop)
        (if (local.get $x)
            (then 
                (return_call $cross_module_callee)
            )
            (else
                (return_call $local_callee)
            )
        )
    )

    (func (export "main") (param $x i32) (result i32)
        (call $foo (local.get $x))
        (table.size 8)
    )
)
`

async function test() {
    const instance = await instantiate(wat1, {}, { simd: true })
    const { cross_module_callee } = instance.exports
    const instance2 = await instantiate(wat2, { o : { cross_module_callee } }, { simd: true, tail_call: true })
    const { main } = instance2.exports

    for (let i = 0; i < 10000; ++i) {
        assert.eq(main(0), 18)
        assert.eq(main(1), 18)
    }
}

await assert.asyncTest(test())
