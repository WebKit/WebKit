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

    ;; Test function: takes an index and a value, calls function via call_ref
    ;; Uses index to select which function reference to call
    (func (export "test") (param $idx i32) (param $val i32) (result i32)
        local.get $val

        ;; Select function reference based on index
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
                ref.func $sub5
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
    0x05, // section length (5 bytes)
    0x04, // 4 functions
    0x00, // func 0: type 0
    0x00, // func 1: type 0
    0x00, // func 2: type 0
    0x01, // func 3: type 1

    // Export section
    0x07, // section code
    0x08, // section length (8 bytes)
    0x01, // 1 export
    0x04, 0x74, 0x65, 0x73, 0x74, // "test" (4 bytes length + "test")
    0x00, 0x03, // export func 3

    // Element section (declare functions for ref.func)
    0x09, // section code
    0x0d, // section length (13 bytes)
    0x01, // 1 element segment
    0x07, // declarative mode with element kind and expressions
    0x70, // element type: funcref (0x70)
    0x03, // 3 elements
    0xd2, 0x00, 0x0b, // ref.func 0, end
    0xd2, 0x01, 0x0b, // ref.func 1, end
    0xd2, 0x02, 0x0b, // ref.func 2, end

    // Code section
    0x0a, // section code
    0x3a, // section length (58 bytes: 1 count + 4*(1 size + body))
    0x04, // 4 functions

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

    // Function 3: test
    0x20, // body size (32 bytes)
    0x00, // 0 locals
    0x20, 0x01, // local.get 1 (val)
    0x20, 0x00, // local.get 0 (idx)
    0x41, 0x00, // i32.const 0
    0x46, // i32.eq
    0x04, 0x64, 0x00, // if (result (ref null 0))
    0xd2, 0x00, // ref.func 0
    0x05, // else
    0x20, 0x00, // local.get 0 (idx)
    0x41, 0x01, // i32.const 1
    0x46, // i32.eq
    0x04, 0x64, 0x00, // if (result (ref null 0))
    0xd2, 0x01, // ref.func 1
    0x05, // else
    0xd2, 0x02, // ref.func 2
    0x0b, // end
    0x0b, // end
    0x14, 0x00, // call_ref 0
    0x0b, // end
]);

async function test() {
    const module = new WebAssembly.Module(bytes);
    const instance = new WebAssembly.Instance(module, {});
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
    // This should exercise polymorphic call_ref profiling
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
