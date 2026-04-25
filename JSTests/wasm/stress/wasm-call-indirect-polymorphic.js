import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (type $sig (func (param i32) (result i32)))
    (table 3 funcref)

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

    ;; Test function: takes an index and a value, calls function at index via call_indirect
    (func (export "test") (param i32 i32) (result i32)
        local.get 1    ;; value parameter
        local.get 0    ;; index parameter
        call_indirect (type $sig)
    )

    ;; Initialize table with the three functions
    (elem (i32.const 0) $mul2 $add10 $sub5)
)
`

async function test() {
    const instance = await instantiate(wat, {}, {});
    const { test } = instance.exports;

    // Test function 0: multiply by 2
    for (let i = 0; i < 100; i++) {
        assert.eq(test(0, 5), 10);
    }

    // Test function 1: add 10
    for (let i = 0; i < 100; i++) {
        assert.eq(test(1, 5), 15);
    }

    // Test function 2: subtract 5
    for (let i = 0; i < 100; i++) {
        assert.eq(test(2, 5), 0);
    }

    // Now test polymorphic behavior: call all three functions in a pattern
    // This should exercise polymorphic call_indirect profiling
    for (let i = 0; i < 1000; i++) {
        const idx = i % 3;
        const value = 20;
        const result = test(idx, value);

        let expected;
        if (idx === 0) {
            expected = value * 2;  // 40
        } else if (idx === 1) {
            expected = value + 10; // 30
        } else {
            expected = value - 5;  // 15
        }

        assert.eq(result, expected);
    }

    for (let outer = 0; outer < testLoopCount; ++outer) {
        // Test with different patterns to ensure polymorphic profiling works correctly
        const patterns = [
            [0, 1, 2],
            [2, 1, 0],
            [0, 0, 1, 1, 2, 2],
            [1, 2, 0],
            [2, 0, 1]
        ];

        for (const pattern of patterns) {
            for (let i = 0; i < 100; i++) {
                const idx = pattern[i % pattern.length];
                const value = 15;

                let expected;
                if (idx === 0) {
                    expected = value * 2;  // 30
                } else if (idx === 1) {
                    expected = value + 10; // 25
                } else {
                    expected = value - 5;  // 10
                }

                assert.eq(test(idx, value), expected);
            }
        }
    }
}

await assert.asyncTest(test())
