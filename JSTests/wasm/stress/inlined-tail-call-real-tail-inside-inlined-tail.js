//@ requireOptions("--jitPolicyScale=0.1", "--useConcurrentJIT=0", "--wasmInliningMaximumWasmCalleeSize=2147483647", "--wasmInliningBudget=1000000", "--wasmInliningMaximumDepth=10", "--wasmInliningMaximumCount=1000", "--wasmFunctionIndexRangeToCompile=8:100")
//@ skip unless $isOMGPlatform
import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

// $mid is reached by an INLINED tail call (from $outer), and $mid itself makes a
// REAL (non-inlined) tail call to $leaf (fn 0, kept out of OMG). $mid's catch must
// NOT fire (mid was tail-called then tail-called away). Correct result = 1.
let wat = `
(module
    (tag $tag)
    (tag $tag2)
    (func $leaf throw $tag)   ;; fn 0, out of compile range -> real tail call
    (func) (func) (func) (func) (func) (func) (func)   ;; fns 1-7 padding
    (func $mid                ;; fn 8
        try
            return_call $leaf
        catch $tag
            throw $tag2
        end)
    (func $outer              ;; fn 9
        return_call $mid)
    (func (export "test") (result i32)   ;; fn 10
        (try (result i32)
            (do
                call $outer
                (i32.const -1))
            (catch $tag (i32.const 1))
            (catch $tag2 (i32.const 2))))
)
`

async function test() {
    const instance = await instantiate(wat, {}, { exceptions: true, tail_call: true })
    const { test } = instance.exports
    for (let i = 0; i < wasmTestLoopCount; ++i) {
        let r = test();
        if (r !== 1) throw new Error(`test=${r} expected 1 (2 means mid's popped catch wrongly fired)`);
    }
}

await assert.asyncTest(test())
