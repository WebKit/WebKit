//@ requireOptions("--jitPolicyScale=0", "--useOMGInlining=1", "--useConcurrentJIT=0", "--wasmInliningMaximumWasmCalleeSize=2147483647", "--wasmInliningBudget=1000000", "--wasmInliningMaximumDepth=10", "--wasmInliningMaximumCount=1000")
//@ skip unless $isOMGPlatform
import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

// A tail call whose *immediate* inlined caller is itself an inlined tail call.
//   test -> call $a (normal inline)
//           $a -> return_call $b (inlined tail call, so $b.frameState == InlinedTailCall)
//                 $b -> return_call $c (inlined tail call; parent codeOrigin is $b)
//                       $c -> throw
// linkExceptionHandlers processes $c's InlinedTailCall codeOrigin and looks up
// its parent (depth 1) which is $b -- also InlinedTailCall.

let wat = `
(module
    (tag $tag)
    (func $c
        throw $tag
    )
    (func $b
        return_call $c
    )
    (func $a
        return_call $b
    )
    (func (export "test") (result i32)
        (try (result i32)
            (do
                call $a
                (i32.const -1))
            (catch $tag (i32.const 1))))
)
`

async function test() {
    const instance = await instantiate(wat, {}, { exceptions: true, tail_call: true })
    const { test } = instance.exports
    for (let i = 0; i < wasmTestLoopCount; ++i) {
        let r = test();
        if (r !== 1) throw new Error(`test=${r} expected 1`);
    }
}

await assert.asyncTest(test())
