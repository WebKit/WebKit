//@ skip if $addressBits <= 32
//@ runDefaultWasm("-m", "--wasmFunctionIndexRangeToCompile=0:5", "--useOMGJIT=0")

import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let watA = `
(module
    (import "o" "functionB" (func $functionB (param $x i32) (result i32)))
    (import "o" "doThrow" (func $doThrow (param i32)))
    (import "o" "log" (func $log (param i32)))

    (memory 1)
    (tag $e)

    (func $fillerSoNoJIT1)
    (func $fillerSoNoJIT2)
    (func $fillerSoNoJIT3)
    (func $fillerSoNoJIT4)
    (func $fillerSoNoJIT5)
    (func $fillerSoNoJIT6)
    (func $fillerSoNoJIT7)
    (func $fillerSoNoJIT8)
    (func $fillerSoNoJIT9)
    (func $fillerSoNoJIT10)

    (func $functionA (export "functionA") (param $x f64) (result f64)
        (local i32)
        (local i32)
        (local i32)
        (local i32)
        (local i32)
        (local i32)
        (local i32)
        (local i32)
        (local i32)
        (local i32)
        (local i32)
        (local i32)
        (local i32)
        (local i32)
        (local i32)
        (local i32)
        (local i32)
        (local i32)
        (local i32)
        (local i32)
        (local i32)
        (local i32)
        (local i32)
        (local i32)
        (local i32)
        (local i32)
        (local i32)
        (local i32)
        (local i32)
        (local i32)
        (local i32)
        (local i32)
        (local i32)
        (local i32)
        (local i32)
        (local $l0 i32)
        (local.set $l0 (i32.const 555555555))

        (if
            (i32.ne
                (i32.load (i32.const 0))
                (i32.const 1337)
            )
        (then
            unreachable
        ))

        (if
            (i32.ge_u
                (i32.trunc_f64_s (local.get $x))
                (i32.const 20)
            )
        (then
            (throw $e)
        ))

        try $ca (result f64)
            (f64.convert_i32_s (call $functionB (i32.trunc_f64_s (f64.add (local.get $x) (f64.const 1)))))
        catch_all
            (if
                (i32.ne
                    (i32.load (i32.const 0))
                    (i32.const 1337)
                )
            (then
                unreachable
            ))
            (if
                (i32.ne
                    (i32.const 555555555)
                    (local.get $l0)
                )
            (then
                unreachable
            ))
            (call $log (i32.const -1))
            (rethrow $ca)
        end

        (if
            (i32.ne
                (i32.load (i32.const 0))
                (i32.const 1337)
            )
        (then
            unreachable
        ))
    )

    (func $setupA (export "setupA")
        (i32.store (i32.const 0) (i32.const 1337))
    )
)
`

let watB = `
(module
    (type $functionA (func (param $x f64) (result f64)))
    (import "o" "tbl" (table $t 1 funcref))
    (import "o" "log" (func $log (param i32)))

    (global $hasBeenCalled (mut i32) (i32.const 0))

    (memory 1)

    (func $functionB (export "functionB") (param $x i32) (result i32)
        (local $shouldCatch i32)
        (local.set $shouldCatch (global.get $hasBeenCalled))
        (global.set $hasBeenCalled (i32.const 1))

        (if
            (i32.ne
                (i32.load (i32.const 0))
                (i32.const 42)
            )
        (then
            unreachable
        ))

        try $ca (result i32)
            (i32.trunc_f64_s (call_indirect (type $functionA) (f64.convert_i32_s (local.get $x)) (i32.const 0)) )
        catch_all
            (if
                (i32.ne
                    (i32.load (i32.const 0))
                    (i32.const 42)
                )
            (then
                unreachable
            ))
            (call $log (i32.const -2))
           (if 
                (i32.ne
                    (local.get $shouldCatch)
                    (i32.const 0)
                )
            (then
                (rethrow $ca)
            ))

            (return (i32.add (local.get $x) (i32.const 6969)))
        end

        (if
            (i32.ne
                (i32.load (i32.const 0))
                (i32.const 42)
            )
        (then
            unreachable
        ))
    )

    (func $fillerSoNoJIT1)
    (func $fillerSoNoJIT2)
    (func $fillerSoNoJIT3)
    (func $fillerSoNoJIT4)
    (func $fillerSoNoJIT5)
    (func $fillerSoNoJIT6)
    (func $fillerSoNoJIT7)
    (func $fillerSoNoJIT8)
    (func $fillerSoNoJIT9)
    (func $fillerSoNoJIT10)

    (func $setupB (export "setupB")
        (global.set $hasBeenCalled (i32.const 0))
        (i32.store (i32.const 0) (i32.const 42))
    )
)
`

function doTest(setupA, setupB, wasmfunctionB) {
    for (let i = 0; i < 500; ++i) {
        setupA()
        setupB()
        assert.eq(wasmfunctionB(1.0), 6970)
    }
}
noInline(doTest)

async function test() {
    function doThrow(i) {
        print("DoThrow ", i)
        let e = new Error("Should be caught")
        print(e.stack)
        throw e
    }

    function log(i) {
        // print("log ", i)
        // let e = new Error("Really should be caught")
        // print(e.stack)
    }

    let tbl = new WebAssembly.Table({ initial: 1, element: "funcref" })

    const instanceB = await instantiate(watB, { o: { tbl, log } }, { simd: true, exceptions: true })
    const { setupB, functionB } = instanceB.exports

    const instanceA = await instantiate(watA, { o: { functionB, doThrow, log } }, { simd: true, exceptions: true })
    const { setupA, functionA } = instanceA.exports

    tbl.set(0, functionA)

    for (let i = 0; i < 50; ++i) {
        doTest(setupA, setupB, functionB)
    }
}

await assert.asyncTest(test())
