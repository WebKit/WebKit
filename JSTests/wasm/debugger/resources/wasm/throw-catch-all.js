// WebAssembly module with comprehensive try-catch_all-throw exception handling tests
// Tests various catch_all handler scenarios:
// 1. Handler in the same function (callee catches its own exception with catch_all)
// 2. Handler in the immediate caller (catch_all in caller)
// 3. Handler in the caller's caller (grandparent with catch_all)
// 4. Handler in the caller's caller's caller (great-grandparent with catch_all)
var wasm = new Uint8Array([
    // [0x00] WASM header
    0x00, 0x61, 0x73, 0x6d, // magic
    0x01, 0x00, 0x00, 0x00, // version

    // [0x08] Type section: 2 function types
    0x01, 0x08,             // section id=1, size=8
    0x02,                   // 2 types
    0x60, 0x00, 0x00,       // Type 0: (func [] -> []) - void function for exception tag
    0x60, 0x00, 0x01, 0x7f, // Type 1: (func [] -> [i32]) - function returning i32

    // [0x12] Function section: 8 functions
    0x03, 0x09,             // section id=3, size=9
    0x08,                   // 8 functions
    0x01,                   // function 0 (thrower): type 1
    0x01,                   // function 1 (catch_all_in_callee): type 1
    0x01,                   // function 2 (no_handler): type 1
    0x01,                   // function 3 (catch_all_from_caller): type 1
    0x01,                   // function 4 (middle_no_handler): type 1
    0x01,                   // function 5 (catch_all_from_grandparent): type 1
    0x01,                   // function 6 (middle_no_handler2): type 1
    0x01,                   // function 7 (test_all): type 1

    // [0x1d] Tag section: 1 exception tag
    0x0d, 0x03,             // section id=13, size=3
    0x01,                   // 1 tag
    0x00, 0x00,             // attribute=0 (exception), type index 0 (void)

    // [0x22] Export section: export function 7 as "test_all"
    0x07, 0x0c,             // section id=7, size=12
    0x01,                   // 1 export
    0x08,                   // name length=8
    0x74, 0x65, 0x73, 0x74, 0x5f, 0x61, 0x6c, 0x6c, // "test_all"
    0x00, 0x07,             // export kind=function, function index=7

    // [0x30] Code section
    0x0a,                   // section id=10
    0x4e,                   // section size=78
    0x08,                   // 8 functions

    // [0x33] Function 0: thrower - throws exception
    // Virtual address base: 0x4000000000000034
    0x06,                   // body size=6
    0x00,                   // 0 locals
    0x08, 0x00,             // [0x35] throw tag=0
    0x41, 0x00,             // [0x37] i32.const 0 (unreachable)
    0x0b,                   // [0x39] end

    // [0x3a] Function 1: catch_all_in_callee - catches own exception with catch_all
    // Virtual address base: 0x400000000000003b
    0x0c,                   // body size=12
    0x00,                   // 0 locals
    0x06, 0x7f,             // [0x3c] try (result i32)
    0x08, 0x00,             // [0x3e] throw tag=0
    0x41, 0x00,             // [0x40] i32.const 0
    0x19,                   // [0x42] catch_all
    0x41, 0x01,             // [0x43] i32.const 1
    0x0b,                   // [0x45] end (try)
    0x0b,                   // [0x46] end (function)

    // [0x47] Function 2: no_handler - calls thrower without handling
    // Virtual address base: 0x4000000000000048
    0x04,                   // body size=4
    0x00,                   // 0 locals
    0x10, 0x00,             // [0x49] call function=0 (thrower)
    0x0b,                   // [0x4b] end

    // [0x4c] Function 3: catch_all_from_caller - catches from callee with catch_all
    // Virtual address base: 0x400000000000004d
    0x0a,                   // body size=10
    0x00,                   // 0 locals
    0x06, 0x7f,             // [0x4e] try (result i32)
    0x10, 0x00,             // [0x50] call function=0 (thrower)
    0x19,                   // [0x52] catch_all
    0x41, 0x02,             // [0x53] i32.const 2
    0x0b,                   // [0x55] end (try)
    0x0b,                   // [0x56] end (function)

    // [0x57] Function 4: middle_no_handler - calls no_handler
    // Virtual address base: 0x4000000000000058
    0x04,                   // body size=4
    0x00,                   // 0 locals
    0x10, 0x02,             // [0x59] call function=2 (no_handler)
    0x0b,                   // [0x5b] end

    // [0x5c] Function 5: catch_all_from_grandparent - catches from 2 levels with catch_all
    // Virtual address base: 0x400000000000005d
    0x0a,                   // body size=10
    0x00,                   // 0 locals
    0x06, 0x7f,             // [0x5e] try (result i32)
    0x10, 0x04,             // [0x60] call function=4 (middle_no_handler)
    0x19,                   // [0x62] catch_all
    0x41, 0x03,             // [0x63] i32.const 3
    0x0b,                   // [0x65] end (try)
    0x0b,                   // [0x66] end (function)

    // [0x67] Function 6: middle_no_handler2 - calls middle_no_handler
    // Virtual address base: 0x4000000000000068
    0x04,                   // body size=4
    0x00,                   // 0 locals
    0x10, 0x04,             // [0x69] call function=4 (middle_no_handler)
    0x0b,                   // [0x6b] end

    // [0x6c] Function 7: test_all - orchestrates all tests with catch_all
    // Virtual address base: 0x400000000000006d
    0x13,                   // body size=19
    0x00,                   // 0 locals
    0x10, 0x01,             // [0x6e] call function=1 (catch_all_in_callee) → returns 1
    0x10, 0x03,             // [0x70] call function=3 (catch_all_from_caller) → returns 2
    0x6a,                   // [0x72] i32.add (1 + 2 = 3)
    0x06, 0x7f,             // [0x73] try (result i32)
    0x10, 0x06,             // [0x75] call function=6 (middle_no_handler2) → throws
    0x19,                   // [0x77] catch_all
    0x41, 0x04,             // [0x78] i32.const 4
    0x0b,                   // [0x7a] end (try)
    0x6a,                   // [0x7b] i32.add (3 + 4 = 7)
    0x10, 0x05,             // [0x7c] call function=5 (catch_all_from_grandparent) → returns 3
    0x6a,                   // [0x7e] i32.add (7 + 3 = 10)
    0x0b,                   // [0x7f] end (function)
    // [0x80] End of code section
]);

var module = new WebAssembly.Module(wasm);
var instance_step = new WebAssembly.Instance(module, {});
let test = instance_step.exports.test_all;

print("DEBUGGER_READY");
let iteration = 0;
for (; ;) {
    let result = test();
    iteration += 1;
    if (iteration % 1e4 == 0) {
        print("iteration=", iteration, "result=", result);
        if (result !== 10) {
            print("ERROR: Expected result=10, got result=", result);
            throw new Error("Test failed");
        }
    }
}
