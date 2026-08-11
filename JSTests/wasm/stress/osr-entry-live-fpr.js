//@ skip unless $isSIMDPlatform
import { instantiate } from "../wabt-wrapper.js"
import * as assert from '../assert.js';

// Verify loop OSR entry with a live FPR, for f32, f64, and v128 types.

function generateWat(type, initialValue) {
  const constOp = `${type}.const`;
  const mulOp = `${type}.mul`;

  return `
(module
  (global $factor (mut ${type}) (${constOp} 1.0))

  (func (export "test") (param $iterations i32) (result ${type})
    (local $counter i32)

    (local.set $counter (i32.const 0))

    ;; Push initial ${type} value onto expression stack
    (${constOp} ${initialValue})

    ;; Loop that keeps the ${type} value on the expression stack
    (loop $continue (param ${type}) (result ${type})
      ;; Stack: [${type}_value]

      ;; Multiply by factor to keep it live and prevent constant folding
      (global.get $factor)
      (${mulOp})
      ;; Stack: [${type}_value * 1.0]

      ;; Increment and check counter
      (local.set $counter (i32.add (local.get $counter) (i32.const 1)))
      (local.get $counter)
      (local.get $iterations)
      (i32.lt_u)
      ;; Stack: [${type}_value, continue?]

      (if (param ${type}) (result ${type})
        (then
          ;; Continue looping - branch back with the ${type} value
          (br $continue)
        )
        (else
          ;; Exit - return the ${type} value
        )
      )
    )
  )
)
`;
}

const watF32 = generateWat('f32', '3.14159274101257324');
const watF64 = generateWat('f64', '2.718281828459045');

// v128 test - uses i32x4.splat and i32x4.add for vector operations
// Returns the sum of all lanes to verify correctness
const watV128 = `
(module
  (global $factor (mut v128) (v128.const i32x4 1 1 1 1))

  (func (export "test") (param $iterations i32) (result i32)
    (local $counter i32)
    (local $result v128)

    ;; Initialize counter
    (local.set $counter (i32.const 0))

    ;; Push initial v128 value onto expression stack (42, 43, 44, 45)
    (v128.const i32x4 42 43 44 45)

    ;; Loop that keeps the v128 value on the expression stack
    (loop $continue (param v128) (result v128)
      ;; Stack: [v128_value]

      ;; Add factor to keep it live and prevent constant folding
      (global.get $factor)
      (i32x4.add)
      ;; Stack: [v128_value + (1,1,1,1)]

      ;; Increment and check counter
      (local.set $counter (i32.add (local.get $counter) (i32.const 1)))
      (local.get $counter)
      (local.get $iterations)
      (i32.lt_u)
      ;; Stack: [v128_value, continue?]

      (if (param v128) (result v128)
        (then
          ;; Continue looping - branch back with the v128 value
          (br $continue)
        )
        (else
          ;; Exit - return the v128 value
        )
      )
    )

    ;; Store result for lane extraction
    (local.set $result)

    ;; Return sum of all 4 lanes: (42 + n) + (43 + n) + (44 + n) + (45 + n)
    ;; where n = iterations
    (i32.add
      (i32.add
        (i32x4.extract_lane 0 (local.get $result))
        (i32x4.extract_lane 1 (local.get $result))
      )
      (i32.add
        (i32x4.extract_lane 2 (local.get $result))
        (i32x4.extract_lane 3 (local.get $result))
      )
    )
  )
)
`;

async function testFloatType(wat, expected, typeName, tolerance) {
    const instance = await instantiate(wat, {});
    const { test } = instance.exports;

    const result = test(wasmTestLoopCount);

    assert.truthy(!isNaN(result), `${typeName} result should not be NaN`);
    assert.truthy(isFinite(result), `${typeName} result should be finite`);
    assert.truthy(Math.abs(result - expected) < tolerance,
                  `${typeName} result should be approximately ${expected}, got ${result}`);
}

async function testV128Type(wat, expected, typeName) {
    const instance = await instantiate(wat, {});
    const { test } = instance.exports;

    const result = test(wasmTestLoopCount);

    // Result is the sum of all 4 lanes
    assert.eq(result, expected,
              `${typeName} sum should be ${expected}, got ${result}`);
}

await assert.asyncTest(testFloatType(watF32, 3.1415927410125732, "f32", 0.0001));
await assert.asyncTest(testFloatType(watF64, 2.718281828459045, "f64", 0.0000001));
// Expected: sum of (42 + n, 43 + n, 44 + n, 45 + n) where n = wasmTestLoopCount
// = (42 + 43 + 44 + 45) + 4 * wasmTestLoopCount = 174 + 4 * wasmTestLoopCount
await assert.asyncTest(testV128Type(watV128, 174 + 4 * wasmTestLoopCount, "v128"));
