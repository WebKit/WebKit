//@ requireOptions("--jitPolicyScale=0.1", "--useConcurrentJIT=0", "--wasmInliningMaximumWasmCalleeSize=2147483647", "--wasmInliningBudget=1000000", "--wasmInliningMaximumDepth=10", "--wasmInliningMaximumCount=1000", "--wasmFunctionIndexRangeToCompile=8:100")
//@ skip unless $isOMGPlatform
import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (tag $inner)

    (global $shouldThrow (mut i32) (i32.const 1))

    ;; padding for jit filter
    (func) (func) (func) (func)
    (func) (func) (func) (func)

    (func $maybeThrow
        (if (i32.eq (global.get $shouldThrow) (i32.const 1))
            (then (throw $inner))))

    (func $inlinee (result i32)
        (try (result i32)
            (do
                (call $maybeThrow)
                (i32.const -1))
            (catch $inner
                (i32.const 42))))

    (func $intermediate (result i32)
        (try (result i32)
            (do
                (return_call $inlinee))
            (catch $inner
                (i32.const 100))))

    (func $test (export "test") (result i32)
        (try (result i32)
            (do
                (call $intermediate))
            (catch $inner
                (i32.const 200))))
)
`

let instance = await instantiate(wat, {}, { exceptions: true, tail_call: true });
let { test } = instance.exports;

for (let i = 0; i < wasmTestLoopCount; ++i)
    assert.eq(test(), 42);
