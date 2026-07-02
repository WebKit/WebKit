//@ skip if !$isSIMDPlatform
import { instantiate } from "../wabt-wrapper.js";
import * as assert from "../assert.js";

// Test case 1: SIMD function inlined into non-SIMD function that catches exception
// The SIMD function throws an exception which should be caught by the caller
async function testSIMDInlinedIntoNonSIMD() {
    const wat = `
    (module
        (tag $exn)
        (global $simd_result (mut v128) (v128.const i32x4 0 0 0 0))
        (global $simd_input (mut v128) (v128.const i32x4 1 2 3 4))
        (func $simd_throw (export "simd_throw") (result i32)
            (global.get $simd_input)

            ;; Throw with v128 on the nested expression stack
            (if
                (i32.const 1)
                (then
                    (throw $exn)
                )
            )

            (global.set $simd_result)
            (i32.const 0)
        )
        (func (export "caller") (result i32)
            ;; Non-SIMD function that calls SIMD function and catches exception
            ;; Put some values on the stack before the try block
            (i32.const 10)
            (i32.const 20)

            (try (result i32)
                (do
                    (call $simd_throw)
                )
                (catch $exn
                    (i32.const 42)
                )
            )

            ;; After catching exception, stack should have: 10, 20, 42
            ;; Add them all together: 10 + 20 + 42 = 72
            (i32.add)
            (i32.add)
        )
    )
    `;

    const instance = await instantiate(wat, {}, { exceptions: true, simd: true });
    const { caller } = instance.exports;

    for (let i = 0; i < wasmTestLoopCount; i++) {
        const result = caller();
        assert.eq(result, 72, "Test case 1: Exception should be caught and return 10 + 20 + 42 = 72");
    }
}

// Test case 2: Non-SIMD function inlined into SIMD function that catches exception
// The non-SIMD function throws an exception which should be caught by the SIMD caller
async function testNonSIMDInlinedIntoSIMD() {
    const wat = `
    (module
        (tag $exn)
        (global $simd_input (mut v128) (v128.const i32x4 1 2 3 4))
        (func $non_simd_throw (export "non_simd_throw") (result i32)
            (local $x i32)
            ;; Throw inside an if block
            (if
                (i32.const 1)
                (then
                    (throw $exn)
                )
            )
            (i32.const 0)
        )
        (func (export "simd_caller") (result i32)
            (local $tmp i32)
            ;; SIMD function that calls non-SIMD function and catches exception
            (global.get $simd_input)

            (try (result i32)
                (do
                    (call $non_simd_throw)
                )
                (catch $exn
                    (i32.const 99)
                )
            )

            (local.set $tmp)
            (i32x4.extract_lane 3)
            (local.get $tmp)
            (i32.add)
        )
    )
    `;

    const instance = await instantiate(wat, {}, { exceptions: true, simd: true });
    const { simd_caller } = instance.exports;

    for (let i = 0; i < wasmTestLoopCount; i++) {
        const result = simd_caller();
        assert.eq(result, 103, "Test case 2: Exception should be caught, return 99 + lane 3 (4) = 103");
    }
}

assert.asyncTest(testSIMDInlinedIntoNonSIMD());
assert.asyncTest(testNonSIMDInlinedIntoSIMD());
