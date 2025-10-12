//@ requireOptions("--useWasmSIMD=1", "--useDollarVM=1", "--useOMGInlining=0")
//@ skip unless $isSIMDPlatform
import { instantiate } from "../wabt-wrapper.js";
import * as assert from "../assert.js";

// Verify exception throwing across tiers, including v128 types

const verbose = false;

let watModule1 = `
(module
    (import "m" "isOMG" (func $isOMG (result i32)))
    (import "m" "logOMG" (func $logOMG (param i32 i32)))
    (import "shared" "vtag" (tag $vtag (param i32 v128 f64)))
    (import "other" "coldCallee" (func $coldCallee))
    (import "other" "hotCallee" (func $hotCallee))

    ;; Test 1: Hot caller (with loop) -> Cold callee (no loop, in different module)
    (func $hotCaller (export "testHotCallerColdCallee") (result i32)
        (local $i i32)
        (local $v128_temp v128)
        (local $f64_temp f64)

        (loop $inner
            local.get $i
            i32.const 1
            i32.add
            local.tee $i
            i32.const 50
            i32.lt_s
            br_if $inner
        )

        ;; Function ID 1 for hotCaller
        (call $logOMG (i32.const 1) (call $isOMG))

        try (result i32)
            call $coldCallee
            i32.const 0
        catch $vtag
            ;; Exception parameters are on stack: i32, v128, f64 (top to bottom)
            ;; Store f64 param in local $f64_temp
            local.set $f64_temp
            ;; Store v128 param in local $v128_temp
            local.set $v128_temp
            ;; i32 param is now on top of stack

            ;; Calculate: i32 + (v128_lane0*1) + (v128_lane1*10) + (v128_lane2*100) + (v128_lane3*1000) + f64
            ;; Start with i32 param (already on stack)

            ;; Add v128 lanes with different weights to detect reordering
            local.get $v128_temp
            i32x4.extract_lane 0
            i32.const 1
            i32.mul
            i32.add

            local.get $v128_temp
            i32x4.extract_lane 1
            i32.const 10
            i32.mul
            i32.add

            local.get $v128_temp
            i32x4.extract_lane 2
            i32.const 100
            i32.mul
            i32.add

            local.get $v128_temp
            i32x4.extract_lane 3
            i32.const 1000
            i32.mul
            i32.add

            ;; Add f64 as i32
            local.get $f64_temp
            f64.trunc
            i32.trunc_f64_s
            i32.add
        end
    )

    ;; Test 2: Cold caller (no loop) -> Hot callee (with loop, in different module)
    (func $coldCaller (export "testColdCallerHotCallee") (result i32)
        (local $v128_temp v128)
        (local $f64_temp f64)
        ;; Function ID 3 for coldCaller
        (call $logOMG (i32.const 3) (call $isOMG))

        try (result i32)
            call $hotCallee
            i32.const 0
        catch $vtag
            ;; Same weighted combination logic for test 2
            ;; Store f64 param
            local.set $f64_temp
            ;; Store v128 param
            local.set $v128_temp
            ;; i32 param is now on stack

            local.get $v128_temp
            i32x4.extract_lane 0
            i32.const 1
            i32.mul
            i32.add

            local.get $v128_temp
            i32x4.extract_lane 1
            i32.const 10
            i32.mul
            i32.add

            local.get $v128_temp
            i32x4.extract_lane 2
            i32.const 100
            i32.mul
            i32.add

            local.get $v128_temp
            i32x4.extract_lane 3
            i32.const 1000
            i32.mul
            i32.add

            local.get $f64_temp
            f64.trunc
            i32.trunc_f64_s
            i32.add
        end
    )
)
`;

let watModule2 = `
(module
    (import "m" "isOMG" (func $isOMG (result i32)))
    (import "m" "logOMG" (func $logOMG (param i32 i32)))
    (import "shared" "vtag" (tag $vtag (param i32 v128 f64)))

    (func $coldCallee (export "coldCallee")
        ;; Function ID 2 for coldCallee
        (call $logOMG (i32.const 2) (call $isOMG))

        ;; Throw complex exception: i32=10, v128=[1,2,3,4], f64=5.0
        i32.const 10
        v128.const i32x4 1 2 3 4
        f64.const 5.0
        throw $vtag
    )

    (func $hotCallee (export "hotCallee")
        (local $i i32)

        (loop $inner
            local.get $i
            i32.const 1
            i32.add
            local.tee $i
            i32.const 50
            i32.lt_s
            br_if $inner
        )

        ;; Function ID 4 for hotCallee
        (call $logOMG (i32.const 4) (call $isOMG))

        ;; Throw complex exception: i32=20, v128=[5,6,7,8], f64=9.0
        i32.const 20
        v128.const i32x4 5 6 7 8
        f64.const 9.0
        throw $vtag
    )
)
`;

async function test() {
    let omgTracker = {};
    let currentIteration = 0;

    function logOMG(funcId, isOMG) {
        if (isOMG && !omgTracker[funcId]) {
            omgTracker[funcId] = true;
            if (verbose)
                print(`Function ${funcId} reached OMG at iteration ${currentIteration}`);
        }
    }

    // Create shared exception tag with multiple parameters
    const sharedTag = new WebAssembly.Tag({ parameters: ['i32', 'v128', 'f64'] });

    // First instantiate module 2 (contains coldCallee and hotCallee)
    const instance2 = await instantiate(watModule2, {
        m: {
            isOMG: $vm.omgTrue,
            logOMG: logOMG
        },
        shared: {
            vtag: sharedTag
        }
    }, { exceptions: true, simd: true });

    // Then instantiate module 1 (contains hotCaller and coldCaller, imports from module 2)
    const instance1 = await instantiate(watModule1, {
        m: {
            isOMG: $vm.omgTrue,
            logOMG: logOMG
        },
        shared: {
            vtag: sharedTag
        },
        other: {
            coldCallee: instance2.exports.coldCallee,
            hotCallee: instance2.exports.hotCallee
        }
    }, { exceptions: true, simd: true });

    for (let i = 0; i < wasmTestLoopCount; i++) {
        currentIteration = i;

        // Test 1: Hot caller (module 1) -> Cold callee (module 2)
        // Expected: 10 + (1*1 + 2*10 + 3*100 + 4*1000) + 5 = 10 + 4321 + 5 = 4336
        assert.eq(instance1.exports.testHotCallerColdCallee(), 4336);

        // Test 2: Cold caller (module 1) -> Hot callee (module 2)
        // Expected: 20 + (5*1 + 6*10 + 7*100 + 8*1000) + 9 = 20 + 8765 + 9 = 8794
        assert.eq(instance1.exports.testColdCallerHotCallee(), 8794);
    }
}

await assert.asyncTest(test());