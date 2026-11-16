//@ requireOptions("--useWasmTailCalls=true", "--wasmInliningMaximumWasmCalleeSize=0", "--maxPerThreadStackUsage=296960")
import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

// Test that repeated cross-instance tail calls reuse the thunk frame rather
// than accumulating new ones. With a 64KB stack limit, if thunks accumulated
// (32 bytes per hop), 5000 hops would need 160KB and overflow. With thunk
// reuse, only one thunk exists so the chain completes at any depth.

let watPing = `
(module
    (type $pp (func (param i32) (result i32)))
    (import "t" "table" (table 2 funcref))
    (memory (export "mem") 1)
    (data (i32.const 0) "\\2a\\00\\00\\00")

    (func (export "ping") (param i32) (result i32)
        (if (result i32) (i32.le_s (local.get 0) (i32.const 0))
            (then (i32.load (i32.const 0)))
            (else
                (i32.sub (local.get 0) (i32.const 1))
                (i32.const 1)
                (return_call_indirect (type $pp))
            )
        )
    )

    (func (export "start") (param i32) (result i32)
        (call 0 (local.get 0))
    )
)
`

let watPong = `
(module
    (type $pp (func (param i32) (result i32)))
    (import "t" "table" (table 2 funcref))
    (memory (export "mem") 1)
    (data (i32.const 0) "\\ff\\00\\00\\00")

    (func (export "pong") (param i32) (result i32)
        (if (result i32) (i32.le_s (local.get 0) (i32.const 0))
            (then (i32.load (i32.const 0)))
            (else
                (i32.sub (local.get 0) (i32.const 1))
                (i32.const 0)
                (return_call_indirect (type $pp))
            )
        )
    )
)
`

async function test() {
    const table = new WebAssembly.Table({ initial: 2, element: "funcref" });
    const instPing = await instantiate(watPing, { t: { table } }, { tail_call: true });
    const instPong = await instantiate(watPong, { t: { table } }, { tail_call: true });
    table.set(0, instPing.exports.ping);
    table.set(1, instPong.exports.pong);

    // 5000 cross-instance hops. Without thunk reuse this would accumulate
    // 160KB of redirect frames, overflowing our 64KB stack.
    assert.eq(instPing.exports.start(5000), 42);
    assert.eq(instPing.exports.start(5001), 255);
}

await assert.asyncTest(test())
