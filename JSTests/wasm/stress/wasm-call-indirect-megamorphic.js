import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (type $sig (func (param i32) (result i32)))
    (table 8 funcref)

    ;; Function 0: multiply by 2
    (func $mul2 (param i32) (result i32)
        local.get 0
        i32.const 2
        i32.mul
    )

    ;; Function 1: add 10
    (func $add10 (param i32) (result i32)
        local.get 0
        i32.const 10
        i32.add
    )

    ;; Function 2: subtract 5
    (func $sub5 (param i32) (result i32)
        local.get 0
        i32.const 5
        i32.sub
    )

    ;; Function 3: multiply by 3
    (func $mul3 (param i32) (result i32)
        local.get 0
        i32.const 3
        i32.mul
    )

    ;; Function 4: add 20
    (func $add20 (param i32) (result i32)
        local.get 0
        i32.const 20
        i32.add
    )

    ;; Function 5: divide by 2
    (func $div2 (param i32) (result i32)
        local.get 0
        i32.const 2
        i32.div_s
    )

    ;; Function 6: negate
    (func $neg (param i32) (result i32)
        i32.const 0
        local.get 0
        i32.sub
    )

    ;; Function 7: add 100
    (func $add100 (param i32) (result i32)
        local.get 0
        i32.const 100
        i32.add
    )

    ;; Test function: takes an index and a value, calls function at index via call_indirect
    (func (export "test") (param i32 i32) (result i32)
        local.get 1    ;; value parameter
        local.get 0    ;; index parameter
        call_indirect (type $sig)
    )

    ;; Initialize table with the eight functions
    (elem (i32.const 0) $mul2 $add10 $sub5 $mul3 $add20 $div2 $neg $add100)
)
`

async function test() {
    const instance = await instantiate(wat, {}, {});
    const { test } = instance.exports;

    // Helper function to compute expected result
    function computeExpected(idx, value) {
        switch(idx) {
            case 0: return value * 2;
            case 1: return value + 10;
            case 2: return value - 5;
            case 3: return value * 3;
            case 4: return value + 20;
            case 5: return Math.trunc(value / 2);
            case 6: return -value;
            case 7: return value + 100;
            default: throw new Error("Invalid index");
        }
    }

    // Test each function individually first
    for (let idx = 0; idx < 8; idx++) {
        for (let i = 0; i < 100; i++) {
            const value = 20;
            const expected = computeExpected(idx, value);
            assert.eq(test(idx, value), expected);
        }
    }

    // Now test megamorphic behavior: call all eight functions in a pattern
    // This should exercise megamorphic call_indirect profiling
    for (let i = 0; i < 1000; i++) {
        const idx = i % 8;
        const value = 24;
        const expected = computeExpected(idx, value);
        assert.eq(test(idx, value), expected);
    }

    for (let outer = 0; outer < testLoopCount; ++outer) {
        // Test with different patterns to ensure megamorphic profiling works correctly
        const patterns = [
            [0, 1, 2, 3, 4, 5, 6, 7],
            [7, 6, 5, 4, 3, 2, 1, 0],
            [0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7],
            [1, 3, 5, 7, 0, 2, 4, 6],
            [7, 3, 1, 5, 2, 6, 0, 4]
        ];

        for (const pattern of patterns) {
            for (let i = 0; i < 100; i++) {
                const idx = pattern[i % pattern.length];
                const value = 30;
                const expected = computeExpected(idx, value);
                assert.eq(test(idx, value), expected);
            }
        }
    }
}

await assert.asyncTest(test())
