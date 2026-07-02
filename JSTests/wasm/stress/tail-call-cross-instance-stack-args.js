//@ requireOptions("--useWasmTailCalls=true", "--wasmInliningMaximumWasmCalleeSize=0")
import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

// Test cross-instance tail calls specifically exercising stack arguments (10+ params),
// indirect calls (call_ref via table), chained A→B→C, and deep recursion for tier-up.

// --- Test 1: Direct import with stack args ---
async function testDirectImportStackArgs() {
    let watCallee = `
    (module
        (memory (export "mem") 1)
        (data (i32.const 0) "\\2a\\00\\00\\00")  ;; memory[0] = 42

        (func (export "big_sum")
            (param i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32) (result i32)
            local.get 0  local.get 1  i32.add
            local.get 2  i32.add  local.get 3  i32.add
            local.get 4  i32.add  local.get 5  i32.add
            local.get 6  i32.add  local.get 7  i32.add
            local.get 8  i32.add  local.get 9  i32.add
            local.get 10 i32.add  local.get 11 i32.add
            (i32.load (i32.const 0))
            i32.add
        )
    )
    `

    let watCaller = `
    (module
        (import "m" "big_sum" (func $big_sum
            (param i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32) (result i32)))

        (memory (export "mem") 1)
        (data (i32.const 0) "\\ff\\00\\00\\00")  ;; memory[0] = 255

        (func $tail_call_big_sum (export "tail_call_big_sum")
            (param i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32) (result i32)
            local.get 0  local.get 1  local.get 2  local.get 3
            local.get 4  local.get 5  local.get 6  local.get 7
            local.get 8  local.get 9  local.get 10 local.get 11
            return_call $big_sum
        )

        ;; Wrapper: call -> tail_call, then check our memory is restored.
        (func (export "call_then_tail")
            (param i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32) (result i32)
            local.get 0  local.get 1  local.get 2  local.get 3
            local.get 4  local.get 5  local.get 6  local.get 7
            local.get 8  local.get 9  local.get 10 local.get 11
            call $tail_call_big_sum
            ;; After return, our instance (caller) must be restored.
            (i32.load (i32.const 0))
            i32.add
        )
    )
    `

    const instCallee = await instantiate(watCallee, {}, { tail_call: true });
    const instCaller = await instantiate(watCaller, {
        m: { big_sum: instCallee.exports.big_sum }
    }, { tail_call: true });

    // sum(0..11) = 66, + callee memory[0]=42 => 108
    for (let i = 0; i < wasmTestLoopCount; ++i) {
        assert.eq(instCaller.exports.tail_call_big_sum(0,1,2,3,4,5,6,7,8,9,10,11), 108);
    }

    // call_then_tail: 108 + caller memory[0]=255 => 363
    for (let i = 0; i < wasmTestLoopCount; ++i) {
        assert.eq(instCaller.exports.call_then_tail(0,1,2,3,4,5,6,7,8,9,10,11), 363);
    }
}

// --- Test 2: Indirect (call_ref via table) with stack args ---
async function testIndirectStackArgs() {
    let watCallee = `
    (module
        (type $big (func (param i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32) (result i32)))
        (memory (export "mem") 1)
        (data (i32.const 0) "\\2a\\00\\00\\00")  ;; memory[0] = 42

        (func (export "big_sum") (type $big)
            local.get 0  local.get 1  i32.add
            local.get 2  i32.add  local.get 3  i32.add
            local.get 4  i32.add  local.get 5  i32.add
            local.get 6  i32.add  local.get 7  i32.add
            local.get 8  i32.add  local.get 9  i32.add
            local.get 10 i32.add  local.get 11 i32.add
            (i32.load (i32.const 0))
            i32.add
        )
    )
    `

    let watCaller = `
    (module
        (type $big (func (param i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32) (result i32)))
        (import "t" "table" (table 1 funcref))

        (memory (export "mem") 1)
        (data (i32.const 0) "\\ff\\00\\00\\00")  ;; memory[0] = 255

        ;; Indirect tail call via table[0].
        (func $indirect_tail (export "indirect_tail")
            (param i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32) (result i32)
            local.get 0  local.get 1  local.get 2  local.get 3
            local.get 4  local.get 5  local.get 6  local.get 7
            local.get 8  local.get 9  local.get 10 local.get 11
            (i32.const 0)
            (return_call_indirect (type $big))
        )

        ;; Wrapper: call -> indirect tail call, then verify memory restored.
        (func (export "call_then_indirect_tail")
            (param i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32) (result i32)
            local.get 0  local.get 1  local.get 2  local.get 3
            local.get 4  local.get 5  local.get 6  local.get 7
            local.get 8  local.get 9  local.get 10 local.get 11
            call $indirect_tail
            (i32.load (i32.const 0))
            i32.add
        )
    )
    `

    const table = new WebAssembly.Table({ initial: 1, element: "funcref" });
    const instCallee = await instantiate(watCallee, {}, { tail_call: true });
    const instCaller = await instantiate(watCaller, { t: { table } }, { tail_call: true });
    table.set(0, instCallee.exports.big_sum);

    // sum(0..11) = 66, + callee memory[0]=42 => 108
    for (let i = 0; i < wasmTestLoopCount; ++i) {
        assert.eq(instCaller.exports.indirect_tail(0,1,2,3,4,5,6,7,8,9,10,11), 108);
    }

    // call_then_indirect_tail: 108 + caller memory[0]=255 => 363
    for (let i = 0; i < wasmTestLoopCount; ++i) {
        assert.eq(instCaller.exports.call_then_indirect_tail(0,1,2,3,4,5,6,7,8,9,10,11), 363);
    }
}

// --- Test 3: Chain A→B→C with stack args (tests hasExistingThunk path) ---
async function testChainABC() {
    let watC = `
    (module
        (memory (export "mem") 1)
        (data (i32.const 0) "\\63\\00\\00\\00")  ;; memory[0] = 99

        (func (export "final_sum")
            (param i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32) (result i32)
            local.get 0  local.get 1  i32.add
            local.get 2  i32.add  local.get 3  i32.add
            local.get 4  i32.add  local.get 5  i32.add
            local.get 6  i32.add  local.get 7  i32.add
            local.get 8  i32.add  local.get 9  i32.add
            local.get 10 i32.add  local.get 11 i32.add
            (i32.load (i32.const 0))
            i32.add
        )
    )
    `

    let watB = `
    (module
        (import "c" "final_sum" (func $c_sum
            (param i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32) (result i32)))

        (memory (export "mem") 1)
        (data (i32.const 0) "\\bb\\00\\00\\00")  ;; memory[0] = 187

        ;; B tail-calls C — second cross-instance hop, hasExistingThunk path.
        (func (export "relay")
            (param i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32) (result i32)
            local.get 0  local.get 1  local.get 2  local.get 3
            local.get 4  local.get 5  local.get 6  local.get 7
            local.get 8  local.get 9  local.get 10 local.get 11
            return_call $c_sum
        )
    )
    `

    let watA = `
    (module
        (import "b" "relay" (func $b_relay
            (param i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32) (result i32)))

        (memory (export "mem") 1)
        (data (i32.const 0) "\\aa\\00\\00\\00")  ;; memory[0] = 170

        ;; A tail-calls B — first cross-instance hop.
        (func $start_chain (export "start_chain")
            (param i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32) (result i32)
            local.get 0  local.get 1  local.get 2  local.get 3
            local.get 4  local.get 5  local.get 6  local.get 7
            local.get 8  local.get 9  local.get 10 local.get 11
            return_call $b_relay
        )

        ;; Wrapper with regular call so memory restoration is tested.
        (func (export "call_start_chain")
            (param i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32) (result i32)
            local.get 0  local.get 1  local.get 2  local.get 3
            local.get 4  local.get 5  local.get 6  local.get 7
            local.get 8  local.get 9  local.get 10 local.get 11
            call $start_chain
            ;; After return, A's instance must be restored.
            (i32.load (i32.const 0))
            i32.add
        )
    )
    `

    const instC = await instantiate(watC, {}, { tail_call: true });
    const instB = await instantiate(watB, { c: { final_sum: instC.exports.final_sum } }, { tail_call: true });
    const instA = await instantiate(watA, { b: { relay: instB.exports.relay } }, { tail_call: true });

    // A -> B -> C: sum(0..11)=66 + C's memory[0]=99 => 165
    for (let i = 0; i < wasmTestLoopCount; ++i) {
        assert.eq(instA.exports.start_chain(0,1,2,3,4,5,6,7,8,9,10,11), 165);
    }

    // call_start_chain: 165 + A's memory[0]=170 => 335
    for (let i = 0; i < wasmTestLoopCount; ++i) {
        assert.eq(instA.exports.call_start_chain(0,1,2,3,4,5,6,7,8,9,10,11), 335);
    }
}

// --- Test 4: Deep recursion with stack args (tier-up test) ---
async function testDeepRecursionStackArgs() {
    let watPing = `
    (module
        (type $rec (func (param i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32) (result i32)))
        (import "t" "table" (table 2 funcref))
        (memory (export "mem") 1)
        (data (i32.const 0) "\\2a\\00\\00\\00")  ;; memory[0] = 42

        (func $ping (export "ping") (type $rec)
            (param i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32) (result i32)
            (if (result i32) (i32.le_s (local.get 0) (i32.const 0))
                (then
                    ;; Base case: return sum of params 1..11 + memory[0]
                    local.get 1  local.get 2  i32.add
                    local.get 3  i32.add  local.get 4  i32.add
                    local.get 5  i32.add  local.get 6  i32.add
                    local.get 7  i32.add  local.get 8  i32.add
                    local.get 9  i32.add  local.get 10 i32.add
                    local.get 11 i32.add
                    (i32.load (i32.const 0))
                    i32.add
                )
                (else
                    ;; Tail call pong (table[1]) with n-1
                    (i32.sub (local.get 0) (i32.const 1))
                    local.get 1  local.get 2  local.get 3
                    local.get 4  local.get 5  local.get 6
                    local.get 7  local.get 8  local.get 9
                    local.get 10 local.get 11
                    (i32.const 1)
                    (return_call_indirect (type $rec))
                )
            )
        )

        (func (export "start") (param i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32) (result i32)
            local.get 0  local.get 1  local.get 2  local.get 3
            local.get 4  local.get 5  local.get 6  local.get 7
            local.get 8  local.get 9  local.get 10 local.get 11
            call $ping
        )
    )
    `

    let watPong = `
    (module
        (type $rec (func (param i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32) (result i32)))
        (import "t" "table" (table 2 funcref))
        (memory (export "mem") 1)
        (data (i32.const 0) "\\ff\\00\\00\\00")  ;; memory[0] = 255

        (func (export "pong") (type $rec)
            (param i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32) (result i32)
            (if (result i32) (i32.le_s (local.get 0) (i32.const 0))
                (then
                    local.get 1  local.get 2  i32.add
                    local.get 3  i32.add  local.get 4  i32.add
                    local.get 5  i32.add  local.get 6  i32.add
                    local.get 7  i32.add  local.get 8  i32.add
                    local.get 9  i32.add  local.get 10 i32.add
                    local.get 11 i32.add
                    (i32.load (i32.const 0))
                    i32.add
                )
                (else
                    (i32.sub (local.get 0) (i32.const 1))
                    local.get 1  local.get 2  local.get 3
                    local.get 4  local.get 5  local.get 6
                    local.get 7  local.get 8  local.get 9
                    local.get 10 local.get 11
                    (i32.const 0)
                    (return_call_indirect (type $rec))
                )
            )
        )
    )
    `

    const table = new WebAssembly.Table({ initial: 2, element: "funcref" });
    const instPing = await instantiate(watPing, { t: { table } }, { tail_call: true });
    const instPong = await instantiate(watPong, { t: { table } }, { tail_call: true });
    table.set(0, instPing.exports.ping);
    table.set(1, instPong.exports.pong);

    // sum(1..11) = 66, + memory[0]:
    // Even n stops in ping (memory=42), odd n stops in pong (memory=255).
    // sum(1..11) = 1+2+3+4+5+6+7+8+9+10+11 = 66
    assert.eq(instPing.exports.start(0,1,2,3,4,5,6,7,8,9,10,11), 66 + 42);  // n=0 base in ping
    assert.eq(instPing.exports.start(1,1,2,3,4,5,6,7,8,9,10,11), 66 + 255); // n=1 base in pong

    // Deep recursion (10000 iterations) — must not stack overflow.
    assert.eq(instPing.exports.start(10000,1,2,3,4,5,6,7,8,9,10,11), 66 + 42);   // even => ping
    assert.eq(instPing.exports.start(10001,1,2,3,4,5,6,7,8,9,10,11), 66 + 255);  // odd => pong
}

await assert.asyncTest(testDirectImportStackArgs());
await assert.asyncTest(testIndirectStackArgs());
await assert.asyncTest(testChainABC());
await assert.asyncTest(testDeepRecursionStackArgs());
