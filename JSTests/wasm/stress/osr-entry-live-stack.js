//@ skip unless $isSIMDPlatform
import { instantiate } from "../wabt-wrapper.js";
import * as assert from "../assert.js";

// Verify loop OSR entry with live values on the stack (not in registers).
// By keeping many values live across the loop without using them inside the loop,
// we encourage them to be stay in stack slots, testing the stack restoration path
// in loop OSR entry.

const wat = `
(module
  (import "env" "sideEffect" (func $sideEffect))

  ;; Globals to prevent constant folding - exported so optimizer can't assume they're constant
  (global $i32_1 (export "i32_1") (mut i32) (i32.const 42))
  (global $i32_2 (export "i32_2") (mut i32) (i32.const 43))
  (global $i32_3 (export "i32_3") (mut i32) (i32.const 44))
  (global $i64_1 (export "i64_1") (mut i64) (i64.const 100))
  (global $i64_2 (export "i64_2") (mut i64) (i64.const 200))
  (global $f32_1 (export "f32_1") (mut f32) (f32.const 3.14159))
  (global $f32_2 (export "f32_2") (mut f32) (f32.const 2.71828))
  (global $f64_1 (export "f64_1") (mut f64) (f64.const 1.41421))
  (global $f64_2 (export "f64_2") (mut f64) (f64.const 1.73205))
  (global $v128_1 (export "v128_1") (mut v128) (v128.const i32x4 10 20 30 40))

  (func (export "test") (param $iterations i32) (result i32)
    (local $counter i32)

    ;; Push multiple values of different types onto the expression stack
    ;; Load from globals to prevent constant folding

    ;; i32 values
    (global.get $i32_1)
    (global.get $i32_2)
    (global.get $i32_3)

    ;; i64 values
    (global.get $i64_1)
    (global.get $i64_2)

    ;; f32 values
    (global.get $f32_1)
    (global.get $f32_2)

    ;; f64 values
    (global.get $f64_1)
    (global.get $f64_2)

    ;; v128 value
    (global.get $v128_1)

    ;; Stack: [i32, i32, i32, i64, i64, f32, f32, f64, f64, v128]

    ;; Initialize counter
    (local.set $counter (i32.const 0))

    ;; Simple loop that only touches the counter
    ;; Call to JS function prevents optimization
    (block $exit
      (loop $continue
        ;; Call JS function to prevent compiler from optimizing away the loop
        (call $sideEffect)

        (local.set $counter (i32.add (local.get $counter) (i32.const 1)))
        (br_if $exit (i32.ge_u (local.get $counter) (local.get $iterations)))
        (br $continue)
      )
    )

    ;; Now verify all values by combining them on the stack
    ;; Stack (bottom to top): [i32(42), i32(43), i32(44), i64(100), i64(200), f32(3.14159), f32(2.71828), f64(1.41421), f64(1.73205), v128(10,20,30,40)]

    ;; Extract lane 3 from v128: 40
    (i32x4.extract_lane 3)
    ;; Stack: [i32(42), i32(43), i32(44), i64(100), i64(200), f32(3.14159), f32(2.71828), f64(1.41421), f64(1.73205), i32(40)]

    ;; Now we need to work from the top down, combining adjacent pairs
    ;; Convert i32 to f64
    (f64.convert_i32_s)
    ;; Stack: [..., f64(1.73205), f64(40.0)]

    ;; Add two f64s: 1.73205 + 40.0 = 41.73205
    (f64.add)
    ;; Stack: [..., f64(1.41421), f64(41.73205)]

    ;; Add two f64s: 1.41421 + 41.73205 = 43.14626
    (f64.add)
    ;; Stack: [..., f32(2.71828), f64(43.14626)]

    ;; Convert f64 to f32 to combine with f32 below
    (f32.demote_f64)
    ;; Stack: [..., f32(2.71828), f32(43.14626)]

    ;; Add two f32s: 2.71828 + 43.14626 = 45.86454
    (f32.add)
    ;; Stack: [..., f32(3.14159), f32(45.86454)]

    ;; Add two f32s: 3.14159 + 45.86454 = 49.00613
    (f32.add)
    ;; Stack: [..., i64(200), f32(49.00613)]

    ;; Truncate f32 to i32: 49.00613 -> 49
    (i32.trunc_f32_s)
    ;; Stack: [..., i64(200), i32(49)]

    ;; Extend i32 to i64
    (i64.extend_i32_s)
    ;; Stack: [..., i64(200), i64(49)]

    ;; Add two i64s: 200 + 49 = 249
    (i64.add)
    ;; Stack: [..., i64(100), i64(249)]

    ;; Add two i64s: 100 + 249 = 349
    (i64.add)
    ;; Stack: [..., i32(44), i64(349)]

    ;; Wrap i64 to i32: 349 -> 349
    (i32.wrap_i64)
    ;; Stack: [..., i32(44), i32(349)]

    ;; Add two i32s: 44 + 349 = 393
    (i32.add)
    ;; Stack: [i32(42), i32(43), i32(393)]

    ;; Add two i32s: 43 + 393 = 436
    (i32.add)
    ;; Stack: [i32(42), i32(436)]

    ;; Add two i32s: 42 + 436 = 478
    (i32.add)
    ;; Stack: [i32(478)]
  )
)
`;

async function test() {
    // Create a JS function that the wasm module will import
    const imports = {
        env: {
            sideEffect: function() { }
        }
    };

    const instance = await instantiate(wat, imports);
    const { test } = instance.exports;

    const result = test(wasmTestLoopCount);

    // Expected: 40 + trunc(43.14626 + 2.71828 + 3.14159) + 200 + 100 + 44 + 43 + 42
    //         = 40 + 49 + 200 + 100 + 44 + 43 + 42 = 478
    const expected = 478;
    assert.eq(result, expected, `Result should be ${expected}, got ${result}`);
}

await assert.asyncTest(test());
