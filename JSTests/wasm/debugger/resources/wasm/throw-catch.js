// WebAssembly module with comprehensive try-catch-throw exception handling tests
// Tests various exception handler scenarios:
// 1. Handler in the same function (callee catches its own exception)
// 2. Handler in the immediate caller
// 3. Handler in the caller's caller (grandparent)
// 4. Handler in the caller's caller's caller (great-grandparent)
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
    0x01,                   // function 1 (catch_in_callee): type 1
    0x01,                   // function 2 (no_handler): type 1
    0x01,                   // function 3 (catch_from_caller): type 1
    0x01,                   // function 4 (middle_no_handler): type 1
    0x01,                   // function 5 (catch_from_grandparent): type 1
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
    0x52,                   // section size=82
    0x08,                   // 8 functions

    // [0x33] Function 0 (thrower): throws exception
    0x06,                   // function body size=6
    0x00,                   // 0 local declarations
    0x08, 0x00,             // [0x35] throw tag 0
    0x41, 0x00,             // [0x37] i32.const 0 (never reached)
    0x0b,                   // [0x39] end

    // [0x3a] Function 1 (catch_in_callee): catches its own exception
    0x0d,                   // function body size=13
    0x00,                   // 0 local declarations
    0x06, 0x7f,             // [0x3c] try (result i32)
    0x08, 0x00,             // [0x3e] throw tag 0
    0x41, 0x00,             // [0x40] i32.const 0 (never reached)
    0x07, 0x00,             // [0x42] catch tag 0
    0x41, 0x01,             // [0x44] i32.const 1 (caught exception)
    0x0b,                   // [0x46] end (try)
    0x0b,                   // [0x47] end (function)

    // [0x48] Function 2 (no_handler): calls thrower without handling
    0x04,                   // function body size=4
    0x00,                   // 0 local declarations
    0x10, 0x00,             // [0x4a] call 0 (thrower)
    0x0b,                   // [0x4c] end

    // [0x4d] Function 3 (catch_from_caller): catches exception from callee
    0x0b,                   // function body size=11
    0x00,                   // 0 local declarations
    0x06, 0x7f,             // [0x4f] try (result i32)
    0x10, 0x00,             // [0x51] call 0 (thrower)
    0x07, 0x00,             // [0x53] catch tag 0
    0x41, 0x02,             // [0x55] i32.const 2 (caught from caller)
    0x0b,                   // [0x57] end (try)
    0x0b,                   // [0x58] end (function)

    // [0x59] Function 4 (middle_no_handler): calls no_handler without handling
    0x04,                   // function body size=4
    0x00,                   // 0 local declarations
    0x10, 0x02,             // [0x5b] call 2 (no_handler)
    0x0b,                   // [0x5d] end

    // [0x5e] Function 5 (catch_from_grandparent): catches from two levels deep
    0x0b,                   // function body size=11
    0x00,                   // 0 local declarations
    0x06, 0x7f,             // [0x60] try (result i32)
    0x10, 0x04,             // [0x62] call 4 (middle_no_handler)
    0x07, 0x00,             // [0x64] catch tag 0
    0x41, 0x03,             // [0x66] i32.const 3 (caught from grandparent)
    0x0b,                   // [0x68] end (try)
    0x0b,                   // [0x69] end (function)

    // [0x6a] Function 6 (middle_no_handler2): another intermediate function
    0x04,                   // function body size=4
    0x00,                   // 0 local declarations
    0x10, 0x04,             // [0x6c] call 4 (middle_no_handler)
    0x0b,                   // [0x6e] end

    // [0x6f] Function 7 (test_all): tests all scenarios and returns sum
    0x14,                   // function body size=20
    0x00,                   // 0 local declarations
    0x10, 0x01,             // [0x71] call 1 (catch_in_callee) - returns 1
    0x10, 0x03,             // [0x73] call 3 (catch_from_caller) - returns 2
    0x6a,                   // [0x75] i32.add (1 + 2 = 3)
    0x06, 0x7f,             // [0x76] try (result i32)
    0x10, 0x06,             // [0x78] call 6 (middle_no_handler2) - throws from 3 levels deep
    0x07, 0x00,             // [0x7a] catch tag 0
    0x41, 0x04,             // [0x7c] i32.const 4 (caught from great-grandparent)
    0x0b,                   // [0x7e] end (try)
    0x6a,                   // [0x7f] i32.add (3 + 4 = 7)
    0x10, 0x05,             // [0x80] call 5 (catch_from_grandparent) - returns 3
    0x6a,                   // [0x82] i32.add (7 + 3 = 10)
    0x0b                    // [0x83] end (function)
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
