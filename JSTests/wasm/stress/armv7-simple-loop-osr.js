//@ requireOptions("--useOMGJIT=1", "--useOMGInlining=0", "--useConcurrentJIT=0", "--useDollarVM=1", "--thresholdForBBQOptimizeAfterWarmUp=0", "--thresholdForBBQOptimizeSoon=0", "--thresholdForOMGOptimizeAfterWarmUp=50", "--thresholdForOMGOptimizeSoon=50")
// jsc -m armv7-simple-loop-osr.js --useOMGJIT=1 --useOMGInlining=0 --useConcurrentJIT=0 --useDollarVM=1 --thresholdForBBQOptimizeAfterWarmUp=0 --thresholdForBBQOptimizeSoon=0 --thresholdForOMGOptimizeAfterWarmUp=50 --thresholdForOMGOptimizeSoon=50
import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (import "m" "isOMG" (func $isOMG (result i32)))
    (import "m" "logOMG" (func $logOMG (param i32)))

    ;; Debugging
    (import "m" "logi32" (func $logi32 (param i32)))
    (import "m" "logi64" (func $logi64 (param i64)))

    (func $test (export "test") (param $l i32) (result i32 i64 i64)
        (local $a i32)
        (local $b i64)
        (local $c i64)
        (local.set $b (i64.const 0x0EEFBAADDEADFEED))
        (local.set $a (i32.const 0x0A00000A))
        (local.set $c (i64.const 0x0B00B00000B))

        (loop $L0
            (local.set $a (i32.add (local.get $a) (i32.const 16)))
            (local.set $b (i64.add (local.get $b) (i64.const 25600000000)))
            (local.set $c (i64.add (local.get $c) (i64.const 3200000000)))
            (call $logOMG (call $isOMG))
            ;; Debugging
            ;; (call $logi32 (i32.const 0xFFFFFFFF))
            ;; (call $logi32 (local.get $a))
            ;; (call $logi64 (local.get $b))
            ;; (call $logi64 (local.get $c))
            ;; (call $logi32 (i32.const 0x77777777))

            (local.set $l (i32.sub (local.get $l) (i32.const 1)))
            (br_if $L0 (i32.ne (local.get $l) (i32.const 0)))
        )

        (local.get $a)
        (local.get $b)
        (local.get $c)
    )
)
`

let hasTierdUp = false

async function test() {
    function log(x) {
        x = BigInt(x)
        // print(x.toString(16))
    }
    function logOMG(x) {
        // print("OMG: " + x)
        assert.truthy(x === 0 || x === 1)
        if (x === 1)
            hasTierdUp = true
    }
    const instance = await instantiate(wat, { m: { isOMG: $vm.omgTrue, logOMG, logi32: log, logi64: log } }, { simd: true, exceptions: true })
    const { test, test_with_call, test_with_call_indirect } = instance.exports

    const result = test(100)
    // print("---")
    log(result[0])
    log(result[1])
    log(result[2])
    assert.eq(result[0], 0xa00064a)
    assert.eq(result[1], 0xeefbd01ea91feedn)
    assert.eq(result[2], 0xfa8c7c800bn)

    assert.truthy(hasTierdUp, "We did not reach OMG")
}

await assert.asyncTest(test())
