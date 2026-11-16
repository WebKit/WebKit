//@ requireOptions("--useWasmTailCalls=true", "--wasmInliningMaximumWasmCalleeSize=0")
import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

// Test cross-instance tail calls with stack arguments/results, memory restoration,
// and deep recursion (no stack overflow).

// Module A: exports a callee that sums many parameters (some on stack) and reads its own memory.
let watA = `
(module
    (memory (export "mem") 1)
    (data (i32.const 0) "\\2a\\00\\00\\00")  ;; memory[0] = 42

    ;; Callee with enough params to force stack usage (>6 GPR args on ARM64, >4 on x86).
    ;; Returns sum of all params + memory[0].
    (func (export "sum_with_memory")
        (param i32 i32 i32 i32 i32 i32 i32 i32 i32 i32) (result i32)
        local.get 0
        local.get 1  i32.add
        local.get 2  i32.add
        local.get 3  i32.add
        local.get 4  i32.add
        local.get 5  i32.add
        local.get 6  i32.add
        local.get 7  i32.add
        local.get 8  i32.add
        local.get 9  i32.add
        ;; Also load from our own memory to verify instance was restored correctly.
        (i32.load (i32.const 0))
        i32.add
    )

    ;; Multi-value callee: returns several values (some on stack).
    (func (export "multi_return") (param i32) (result i32 i32 i32 i32 i32)
        (i32.add (local.get 0) (i32.const 1))
        (i32.add (local.get 0) (i32.const 2))
        (i32.add (local.get 0) (i32.const 3))
        (i32.add (local.get 0) (i32.const 4))
        ;; Last result reads memory to verify correct instance.
        (i32.load (i32.const 0))
    )
)
`

// Module B: has its own memory, imports A's functions, and tail-calls them.
let watB = `
(module
    (import "a" "sum_with_memory" (func $imported_sum
        (param i32 i32 i32 i32 i32 i32 i32 i32 i32 i32) (result i32)))
    (import "a" "multi_return" (func $imported_multi (param i32) (result i32 i32 i32 i32 i32)))
    (import "a" "countdown" (func $imported_countdown (param i32) (result i32)))

    (memory (export "mem") 1)
    (data (i32.const 0) "\\ff\\00\\00\\00")  ;; memory[0] = 255 (different from A's 42)

    ;; call -> return_call import: caller calls us, we tail-call into A's function.
    (func $do_tail_call_sum (export "tail_call_sum")
        (param i32 i32 i32 i32 i32 i32 i32 i32 i32 i32) (result i32)
        local.get 0  local.get 1  local.get 2  local.get 3  local.get 4
        local.get 5  local.get 6  local.get 7  local.get 8  local.get 9
        return_call $imported_sum
    )

    ;; Wrapper that calls the tail-calling function (tests call -> tail_call -> cross-instance).
    (func (export "call_then_tail_call_sum")
        (param i32 i32 i32 i32 i32 i32 i32 i32 i32 i32) (result i32)
        local.get 0  local.get 1  local.get 2  local.get 3  local.get 4
        local.get 5  local.get 6  local.get 7  local.get 8  local.get 9
        call $do_tail_call_sum
        ;; After the call returns, B's memory must be accessible again.
        (i32.load (i32.const 0))
        i32.add
    )

    ;; Tail-call the multi-return import.
    (func $tail_call_multi_internal (export "tail_call_multi") (param i32) (result i32 i32 i32 i32 i32)
        local.get 0
        return_call $imported_multi
    )

    ;; Wrapper: call -> tail_call multi-value cross-instance.
    (func (export "call_then_tail_call_multi") (param i32) (result i32 i32 i32 i32 i32)
        local.get 0
        call $tail_call_multi_internal
    )

    ;; Deep tail-call recursion: counts down via tail calls, alternating cross-instance.
    (func (export "countdown") (param i32) (result i32)
        (if (result i32) (i32.le_s (local.get 0) (i32.const 0))
            (then
                (i32.load (i32.const 0))
            )
            (else
                (i32.sub (local.get 0) (i32.const 1))
                return_call $imported_countdown
            )
        )
    )
)
`

// Module A also needs a countdown that tail-calls back to B.
let watA_with_countdown = `
(module
    (import "b" "countdown" (func $b_countdown (param i32) (result i32)))

    (memory (export "mem") 1)
    (data (i32.const 0) "\\2a\\00\\00\\00")  ;; memory[0] = 42

    (func (export "sum_with_memory")
        (param i32 i32 i32 i32 i32 i32 i32 i32 i32 i32) (result i32)
        local.get 0
        local.get 1  i32.add
        local.get 2  i32.add
        local.get 3  i32.add
        local.get 4  i32.add
        local.get 5  i32.add
        local.get 6  i32.add
        local.get 7  i32.add
        local.get 8  i32.add
        local.get 9  i32.add
        (i32.load (i32.const 0))
        i32.add
    )

    (func (export "multi_return") (param i32) (result i32 i32 i32 i32 i32)
        (i32.add (local.get 0) (i32.const 1))
        (i32.add (local.get 0) (i32.const 2))
        (i32.add (local.get 0) (i32.const 3))
        (i32.add (local.get 0) (i32.const 4))
        (i32.load (i32.const 0))
    )

    (func (export "countdown") (param i32) (result i32)
        (if (result i32) (i32.le_s (local.get 0) (i32.const 0))
            (then
                (i32.load (i32.const 0))
            )
            (else
                (i32.sub (local.get 0) (i32.const 1))
                return_call $b_countdown
            )
        )
    )
)
`

async function test() {
    // --- Set up mutually-recursive modules A and B ---
    // A imports B's countdown, B imports A's functions.
    // We use a two-phase approach: instantiate A with a JS trampoline for B's countdown,
    // then instantiate B with A's exports, then patch A's import.

    // Phase 1: Instantiate A with a placeholder for B's countdown.
    let bCountdownRef = { fn: null };
    const instanceA = await instantiate(watA_with_countdown, {
        b: { countdown: (n) => bCountdownRef.fn(n) }
    }, { tail_call: true });

    // Phase 2: Instantiate B with A's exports.
    const instanceB = await instantiate(watB, {
        a: {
            sum_with_memory: instanceA.exports.sum_with_memory,
            multi_return: instanceA.exports.multi_return,
            countdown: instanceA.exports.countdown,
        }
    }, { tail_call: true });

    // Patch the trampoline.
    bCountdownRef.fn = instanceB.exports.countdown;

    const { call_then_tail_call_sum, tail_call_sum, tail_call_multi, call_then_tail_call_multi } = instanceB.exports;
    const aCountdown = instanceA.exports.countdown;

    // --- Test 1: Stack arguments through cross-instance tail call ---
    // sum(0..9) = 45, plus A's memory[0] = 42 => 87.
    for (let i = 0; i < wasmTestLoopCount; ++i) {
        assert.eq(tail_call_sum(0, 1, 2, 3, 4, 5, 6, 7, 8, 9), 87);
    }

    // --- Test 2: call -> tail_call -> cross-instance, memory restored ---
    // sum = 87 (from A), plus B's memory[0] = 255 (loaded after return) => 342.
    for (let i = 0; i < wasmTestLoopCount; ++i) {
        assert.eq(call_then_tail_call_sum(0, 1, 2, 3, 4, 5, 6, 7, 8, 9), 342);
    }

    // --- Test 3: Multi-value return through cross-instance tail call ---
    for (let i = 0; i < wasmTestLoopCount; ++i) {
        let results = tail_call_multi(10);
        assert.eq(results[0], 11);
        assert.eq(results[1], 12);
        assert.eq(results[2], 13);
        assert.eq(results[3], 14);
        assert.eq(results[4], 42);  // A's memory[0]
    }

    // --- Test 4: call -> tail_call multi-value cross-instance ---
    for (let i = 0; i < wasmTestLoopCount; ++i) {
        let results = call_then_tail_call_multi(10);
        assert.eq(results[0], 11);
        assert.eq(results[1], 12);
        assert.eq(results[2], 13);
        assert.eq(results[3], 14);
        assert.eq(results[4], 42);  // A's memory[0]
    }

    // --- Test 5: Deep mutual tail-call recursion (must not stack overflow) ---
    // A.countdown(n) tail-calls B.countdown(n-1), which tail-calls A.countdown(n-2), etc.
    // At n=0 the base case returns memory[0]. Even n stops in A (returns 42),
    // odd n stops in B (returns 255).
    // Note: A→B goes through a JS trampoline (import resolved at instantiation time).
    assert.eq(aCountdown(0), 42);
    assert.eq(aCountdown(1), 255);
    assert.eq(aCountdown(100), 42);    // moderate depth, even => A's memory
    assert.eq(aCountdown(101), 255);   // moderate depth, odd => B's memory

}

async function testPingPong() {
    // Ping and Pong are in different instances and tail-call each other via a shared table.
    // Both directions are wasm-to-wasm return_call_indirect. Deep recursion must not
    // overflow — repeated cross-instance tail calls must reuse the thunk frame.
    let watPing = `
    (module
        (type $pp (func (param i32) (result i32)))
        (import "t" "table" (table 2 funcref))
        (memory (export "mem") 1)
        (data (i32.const 0) "\\2a\\00\\00\\00")  ;; memory[0] = 42

        ;; ping: if n<=0 return memory[0], else return_call_indirect table[1](n-1) (= pong)
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

        ;; start: regular call to ping so there's a non-tail caller on the stack
        (func (export "start") (param i32) (result i32)
            (call 0 (local.get 0))
        )
    )
    `

    let watPong = `
    (module
        (type $pp (func (param i32) (result i32)))
        (import "t" "table" (table 2 funcref))
        (import "t" "breaker" (func))
        (memory (export "mem") 1)
        (data (i32.const 0) "\\ff\\00\\00\\00")  ;; memory[0] = 255

        ;; pong: if n<=0 return memory[0], else return_call_indirect table[0](n-1) (= ping)
        (func (export "pong") (param i32) (result i32)
            (if (result i32) (i32.le_s (local.get 0) (i32.const 0))
                (then (call 0) (i32.load (i32.const 0)))
                (else
                    (i32.sub (local.get 0) (i32.const 1))
                    (i32.const 0)
                    (return_call_indirect (type $pp))
                )
            )
        )
    )
    `

    const table = new WebAssembly.Table({ initial: 2, element: "funcref" });
    function breaker() {
    }
    const instPing = await instantiate(watPing, { t: { table } }, { tail_call: true });
    const instPong = await instantiate(watPong, { t: { table, breaker } }, { tail_call: true });

    // table[0] = ping, table[1] = pong
    table.set(0, instPing.exports.ping);
    table.set(1, instPong.exports.pong);

    // start(n) -> ping(n) -> pong(n-1) -> ping(n-2) -> ... (all wasm tail calls)
    assert.eq(instPing.exports.start(0), 42);    // stops in Ping
    assert.eq(instPing.exports.start(1), 255);   // stops in Pong
    assert.eq(instPing.exports.start(100), 42);  // moderate depth, even => Ping's memory
    assert.eq(instPing.exports.start(101), 255); // moderate depth, odd => Pong's memory


    // Run near the stack limit to ensure that cross-instance tail calls don't accumulate
    // thunk frames. If each cross-instance hop created a new 32-byte thunk, even moderate
    // recursion would overflow when we're already close to the limit.
    function runNearStackLimit(f) {
        function t() {
            try { return t(); } catch (e) { return f(); }
        }
        return t();
    }
    runNearStackLimit(() => {
        assert.eq(instPing.exports.start(1000), 42);
    });

}

await assert.asyncTest(test())
await assert.asyncTest(testPingPong())

// --- Test: Verify instance (not just memory) is restored via globals ---
async function testInstanceRestoredViaGlobals() {
    let watCallee = `
    (module
        (func (export "identity") (param i32) (result i32)
            local.get 0
        )
    )`

    let watCaller = `
    (module
        (import "m" "identity" (func $identity (param i32) (result i32)))
        (global $marker (mut i32) (i32.const 99))
        (func $tail_identity (export "tail_identity") (param i32) (result i32)
            local.get 0
            return_call $identity
        )
        (func (export "call_then_check_global") (param i32) (result i32)
            local.get 0
            call $tail_identity
            drop
            global.get $marker
        )
    )`

    const instCallee = await instantiate(watCallee, {}, { tail_call: true });
    const instCaller = await instantiate(watCaller, {
        m: { identity: instCallee.exports.identity }
    }, { tail_call: true });

    for (let i = 0; i < wasmTestLoopCount; ++i) {
        assert.eq(instCaller.exports.call_then_check_global(42), 99);
    }
}

await assert.asyncTest(testInstanceRestoredViaGlobals())
