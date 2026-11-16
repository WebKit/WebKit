import * as assert from "../assert.js"

/*
Original WAT format (not supported by wabt due to WasmGC features):

(module
    (type $sig (func (param i32) (result i32)))

    ;; Function 0: multiply by 2
    (func $mul2 (type $sig) (param i32) (result i32)
        local.get 0
        i32.const 2
        i32.mul
    )

    ;; Function 1: add 10
    (func $add10 (type $sig) (param i32) (result i32)
        local.get 0
        i32.const 10
        i32.add
    )

    ;; Function 2: subtract 5
    (func $sub5 (type $sig) (param i32) (result i32)
        local.get 0
        i32.const 5
        i32.sub
    )

    ;; Function 3: multiply by 3
    (func $mul3 (type $sig) (param i32) (result i32)
        local.get 0
        i32.const 3
        i32.mul
    )

    ;; Function 4: add 20
    (func $add20 (type $sig) (param i32) (result i32)
        local.get 0
        i32.const 20
        i32.add
    )

    ;; Function 5: divide by 2
    (func $div2 (type $sig) (param i32) (result i32)
        local.get 0
        i32.const 2
        i32.div_s
    )

    ;; Function 6: negate
    (func $neg (type $sig) (param i32) (result i32)
        i32.const 0
        local.get 0
        i32.sub
    )

    ;; Function 7: add 100
    (func $add100 (type $sig) (param i32) (result i32)
        local.get 0
        i32.const 100
        i32.add
    )

    ;; Test function: takes an index and a value, calls function via call_ref
    ;; Uses index to select which function reference to call
    (func (export "test") (param $idx i32) (param $val i32) (result i32)
        local.get $val

        ;; Select function reference based on index (using nested if-else)
        local.get $idx
        i32.const 0
        i32.eq
        if (result (ref null $sig))
            ref.func $mul2
        else
            local.get $idx
            i32.const 1
            i32.eq
            if (result (ref null $sig))
                ref.func $add10
            else
                local.get $idx
                i32.const 2
                i32.eq
                if (result (ref null $sig))
                    ref.func $sub5
                else
                    local.get $idx
                    i32.const 3
                    i32.eq
                    if (result (ref null $sig))
                        ref.func $mul3
                    else
                        local.get $idx
                        i32.const 4
                        i32.eq
                        if (result (ref null $sig))
                            ref.func $add20
                        else
                            local.get $idx
                            i32.const 5
                            i32.eq
                            if (result (ref null $sig))
                                ref.func $div2
                            else
                                local.get $idx
                                i32.const 6
                                i32.eq
                                if (result (ref null $sig))
                                    ref.func $neg
                                else
                                    ref.func $add100
                                end
                            end
                        end
                    end
                end
            end
        end

        ;; Call the selected function reference
        call_ref $sig
    )
)
*/

let bytes = new Uint8Array([
    0x00, 0x61, 0x73, 0x6d, // magic
    0x01, 0x00, 0x00, 0x00, // version

    // Type section
    0x01, // section code
    0x0c, // section length (12 bytes)
    0x02, // 2 types
    // Type 0: (i32) -> i32
    0x60, // func type
    0x01, 0x7f, // 1 param: i32
    0x01, 0x7f, // 1 result: i32
    // Type 1: (i32, i32) -> i32
    0x60, // func type
    0x02, 0x7f, 0x7f, // 2 params: i32, i32
    0x01, 0x7f, // 1 result: i32

    // Function section
    0x03, // section code
    0x0a, // section length (10 bytes)
    0x09, // 9 functions
    0x00, // func 0: type 0
    0x00, // func 1: type 0
    0x00, // func 2: type 0
    0x00, // func 3: type 0
    0x00, // func 4: type 0
    0x00, // func 5: type 0
    0x00, // func 6: type 0
    0x00, // func 7: type 0
    0x01, // func 8: type 1 (test function)

    // Export section
    0x07, // section code
    0x08, // section length (8 bytes)
    0x01, // 1 export
    0x04, 0x74, 0x65, 0x73, 0x74, // "test" (4 bytes length + "test")
    0x00, 0x08, // export func 8

    // Element section (declare functions for ref.func)
    0x09, // section code
    0x1c, // section length (28 bytes: 1 count + 1 mode + 1 type + 1 count + 8*3 bytes)
    0x01, // 1 element segment
    0x07, // declarative mode with element kind and expressions
    0x70, // element type: funcref (0x70)
    0x08, // 8 elements
    0xd2, 0x00, 0x0b, // ref.func 0, end
    0xd2, 0x01, 0x0b, // ref.func 1, end
    0xd2, 0x02, 0x0b, // ref.func 2, end
    0xd2, 0x03, 0x0b, // ref.func 3, end
    0xd2, 0x04, 0x0b, // ref.func 4, end
    0xd2, 0x05, 0x0b, // ref.func 5, end
    0xd2, 0x06, 0x0b, // ref.func 6, end
    0xd2, 0x07, 0x0b, // ref.func 7, end

    // Code section
    0x0a, // section code
    0x9f, 0x01, // section length (159 bytes using LEB128: 1 + 8*8 + 1 + 93)
    0x09, // 9 functions

    // Function 0: mul2
    0x07, // body size (7 bytes)
    0x00, // 0 locals
    0x20, 0x00, // local.get 0
    0x41, 0x02, // i32.const 2
    0x6c, // i32.mul
    0x0b, // end

    // Function 1: add10
    0x07, // body size (7 bytes)
    0x00, // 0 locals
    0x20, 0x00, // local.get 0
    0x41, 0x0a, // i32.const 10
    0x6a, // i32.add
    0x0b, // end

    // Function 2: sub5
    0x07, // body size (7 bytes)
    0x00, // 0 locals
    0x20, 0x00, // local.get 0
    0x41, 0x05, // i32.const 5
    0x6b, // i32.sub
    0x0b, // end

    // Function 3: mul3
    0x07, // body size (7 bytes)
    0x00, // 0 locals
    0x20, 0x00, // local.get 0
    0x41, 0x03, // i32.const 3
    0x6c, // i32.mul
    0x0b, // end

    // Function 4: add20
    0x07, // body size (7 bytes)
    0x00, // 0 locals
    0x20, 0x00, // local.get 0
    0x41, 0x14, // i32.const 20
    0x6a, // i32.add
    0x0b, // end

    // Function 5: div2
    0x07, // body size (7 bytes)
    0x00, // 0 locals
    0x20, 0x00, // local.get 0
    0x41, 0x02, // i32.const 2
    0x6d, // i32.div_s
    0x0b, // end

    // Function 6: neg
    0x07, // body size (7 bytes)
    0x00, // 0 locals
    0x41, 0x00, // i32.const 0
    0x20, 0x00, // local.get 0
    0x6b, // i32.sub
    0x0b, // end

    // Function 7: add100
    0x08, // body size (8 bytes)
    0x00, // 0 locals
    0x20, 0x00, // local.get 0
    0x41, 0xe4, 0x00, // i32.const 100 (LEB128)
    0x6a, // i32.add
    0x0b, // end

    // Function 8: test (with nested if-else for 8 functions)
    0x5c, // body size (92 bytes)
    0x00, // 0 locals
    0x20, 0x01, // local.get 1 (val)
    // idx == 0?
    0x20, 0x00, // local.get 0 (idx)
    0x41, 0x00, // i32.const 0
    0x46, // i32.eq
    0x04, 0x64, 0x00, // if (result (ref null 0))
    0xd2, 0x00, // ref.func 0
    0x05, // else
    // idx == 1?
    0x20, 0x00, // local.get 0 (idx)
    0x41, 0x01, // i32.const 1
    0x46, // i32.eq
    0x04, 0x64, 0x00, // if (result (ref null 0))
    0xd2, 0x01, // ref.func 1
    0x05, // else
    // idx == 2?
    0x20, 0x00, // local.get 0 (idx)
    0x41, 0x02, // i32.const 2
    0x46, // i32.eq
    0x04, 0x64, 0x00, // if (result (ref null 0))
    0xd2, 0x02, // ref.func 2
    0x05, // else
    // idx == 3?
    0x20, 0x00, // local.get 0 (idx)
    0x41, 0x03, // i32.const 3
    0x46, // i32.eq
    0x04, 0x64, 0x00, // if (result (ref null 0))
    0xd2, 0x03, // ref.func 3
    0x05, // else
    // idx == 4?
    0x20, 0x00, // local.get 0 (idx)
    0x41, 0x04, // i32.const 4
    0x46, // i32.eq
    0x04, 0x64, 0x00, // if (result (ref null 0))
    0xd2, 0x04, // ref.func 4
    0x05, // else
    // idx == 5?
    0x20, 0x00, // local.get 0 (idx)
    0x41, 0x05, // i32.const 5
    0x46, // i32.eq
    0x04, 0x64, 0x00, // if (result (ref null 0))
    0xd2, 0x05, // ref.func 5
    0x05, // else
    // idx == 6?
    0x20, 0x00, // local.get 0 (idx)
    0x41, 0x06, // i32.const 6
    0x46, // i32.eq
    0x04, 0x64, 0x00, // if (result (ref null 0))
    0xd2, 0x06, // ref.func 6
    0x05, // else
    0xd2, 0x07, // ref.func 7
    0x0b, // end
    0x0b, // end
    0x0b, // end
    0x0b, // end
    0x0b, // end
    0x0b, // end
    0x0b, // end
    0x14, 0x00, // call_ref 0
    0x0b, // end
]);

async function test() {
    const module = new WebAssembly.Module(bytes);
    const instance = new WebAssembly.Instance(module, {});
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
    // This should exercise megamorphic call_ref profiling
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
