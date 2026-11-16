// WebAssembly module with comprehensive try_table exception handling tests
// Tests catch kinds 0x00 (catch without exnref) and 0x02 (catch_all without exnref)
// Tests exception propagation through call stack:
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

    // [0x12] Function section: 10 functions
    0x03, 0x0b,             // section id=3, size=11
    0x0a,                   // 10 functions
    0x01,                   // function 0 (thrower): type 1
    0x01,                   // function 1 (catch_in_callee): type 1
    0x01,                   // function 2 (no_handler): type 1
    0x01,                   // function 3 (catch_from_caller): type 1
    0x01,                   // function 4 (middle_no_handler): type 1
    0x01,                   // function 5 (catch_from_grandparent): type 1
    0x01,                   // function 6 (middle_no_handler2): type 1
    0x01,                   // function 7 (catch_all_in_callee): type 1
    0x01,                   // function 8 (catch_all_from_caller): type 1
    0x01,                   // function 9 (test_all): type 1

    // [0x1f] Tag section: 1 exception tag
    0x0d, 0x03,             // section id=13, size=3
    0x01,                   // 1 tag
    0x00, 0x00,             // attribute=0 (exception), type index 0 (void)

    // [0x24] Export section: export function 9 as "test_all"
    0x07, 0x0c,             // section id=7, size=12
    0x01,                   // 1 export
    0x08,                   // name length=8
    0x74, 0x65, 0x73, 0x74, 0x5f, 0x61, 0x6c, 0x6c, // "test_all"
    0x00, 0x09,             // export kind=function, function index=9

    // [0x32] Code section
    0x0a,                   // section id=10
    0xa3, 0x01,             // section size=163
    0x0a,                   // 10 functions

    // [0x36] Function 0 (thrower): throws exception
    0x06,                   // function body size=6
    0x00,                   // 0 local declarations
    0x08, 0x00,             // [0x38] throw tag 0
    0x41, 0x00,             // [0x3a] i32.const 0 (never reached)
    0x0b,                   // [0x3c] end

    // [0x3d] Function 1 (catch_in_callee): catches its own exception with try_table/catch (kind 0x00)
    0x13,                   // function body size=19
    0x00,                   // 0 local declarations
    0x02, 0x40,             // [0x3f] block $catch_handler (no result)
    0x1f, 0x7f,             // [0x41] try_table (result i32)
    0x01,                   // 1 catch clause
    0x00, 0x00, 0x00,       // catch kind=0x00 (catch $exn), tag=0, label=0 ($catch_handler)
    0x08, 0x00,             // [0x47] throw tag 0
    0x41, 0x00,             // [0x49] i32.const 0 (never reached)
    0x0b,                   // [0x4b] end (try_table)
    0x0f,                   // [0x4c] return (normal path, i32.const 0 on stack)
    0x0b,                   // [0x4d] end (block $catch_handler)
    0x41, 0x01,             // [0x4e] i32.const 1 (exception caught)
    0x0b,                   // [0x50] end (function)

    // [0x51] Function 2 (no_handler): calls thrower without handling
    0x04,                   // function body size=4
    0x00,                   // 0 local declarations
    0x10, 0x00,             // [0x53] call 0 (thrower)
    0x0b,                   // [0x55] end

    // [0x56] Function 3 (catch_from_caller): catches exception from callee with try_table/catch (kind 0x00)
    0x11,                   // function body size=17
    0x00,                   // 0 local declarations
    0x02, 0x40,             // [0x58] block $catch_handler (no result)
    0x1f, 0x7f,             // [0x5a] try_table (result i32)
    0x01,                   // 1 catch clause
    0x00, 0x00, 0x00,       // catch kind=0x00 (catch $exn), tag=0, label=0 ($catch_handler)
    0x10, 0x00,             // [0x60] call 0 (thrower)
    0x0b,                   // [0x62] end (try_table)
    0x0f,                   // [0x63] return (normal path)
    0x0b,                   // [0x64] end (block $catch_handler)
    0x41, 0x02,             // [0x65] i32.const 2 (exception caught from caller)
    0x0b,                   // [0x67] end (function)

    // [0x68] Function 4 (middle_no_handler): calls no_handler without handling
    0x04,                   // function body size=4
    0x00,                   // 0 local declarations
    0x10, 0x02,             // [0x6a] call 2 (no_handler)
    0x0b,                   // [0x6c] end

    // [0x6d] Function 5 (catch_from_grandparent): catches from two levels deep with try_table/catch (kind 0x00)
    0x11,                   // function body size=17
    0x00,                   // 0 local declarations
    0x02, 0x40,             // [0x6f] block $catch_handler (no result)
    0x1f, 0x7f,             // [0x71] try_table (result i32)
    0x01,                   // 1 catch clause
    0x00, 0x00, 0x00,       // catch kind=0x00 (catch $exn), tag=0, label=0 ($catch_handler)
    0x10, 0x04,             // [0x77] call 4 (middle_no_handler)
    0x0b,                   // [0x79] end (try_table)
    0x0f,                   // [0x7a] return (normal path)
    0x0b,                   // [0x7b] end (block $catch_handler)
    0x41, 0x03,             // [0x7c] i32.const 3 (exception caught from grandparent)
    0x0b,                   // [0x7e] end (function)

    // [0x7f] Function 6 (middle_no_handler2): another intermediate function
    0x04,                   // function body size=4
    0x00,                   // 0 local declarations
    0x10, 0x04,             // [0x81] call 4 (middle_no_handler)
    0x0b,                   // [0x83] end

    // [0x84] Function 7 (catch_all_in_callee): catches any exception with try_table/catch_all (kind 0x02)
    0x12,                   // function body size=18
    0x00,                   // 0 local declarations
    0x02, 0x40,             // [0x86] block $catch_handler (no result)
    0x1f, 0x40,             // [0x88] try_table (no result)
    0x01,                   // 1 catch clause
    0x02, 0x00,             // catch kind=0x02 (catch_all), label=0 ($catch_handler)
    0x08, 0x00,             // [0x8d] throw tag 0
    0x41, 0x00,             // [0x8f] i32.const 0 (never reached)
    0x1a,                   // [0x91] drop (never reached)
    0x0b,                   // [0x92] end (try_table)
    0x0b,                   // [0x93] end (block $catch_handler)
    0x41, 0x05,             // [0x94] i32.const 5 (exception caught by catch_all)
    0x0b,                   // [0x96] end (function)

    // [0x97] Function 8 (catch_all_from_caller): catches exception from callee with try_table/catch_all (kind 0x02)
    0x10,                   // function body size=16
    0x00,                   // 0 local declarations
    0x02, 0x40,             // [0x99] block $catch_handler (no result)
    0x1f, 0x40,             // [0x9b] try_table (no result)
    0x01,                   // 1 catch clause
    0x02, 0x00,             // catch kind=0x02 (catch_all), label=0 ($catch_handler)
    0x10, 0x00,             // [0xa0] call 0 (thrower)
    0x1a,                   // [0xa2] drop (never reached)
    0x0b,                   // [0xa3] end (try_table)
    0x0b,                   // [0xa4] end (block $catch_handler)
    0x41, 0x06,             // [0xa5] i32.const 6 (exception caught from caller by catch_all)
    0x0b,                   // [0xa7] end (function)

    // [0xa8] Function 9 (test_all): tests all try_table scenarios
    // Normal path: 1 + 2 + (exception at middle_no_handler2, so skips normal path)
    // Exception path: drop(3) + 4 + 3 + 5 + 6 = 21
    0x2f,                   // function body size=47
    0x00,                   // 0 local declarations
    0x10, 0x01,             // [0xaa] call 1 (catch_in_callee) - returns 1
    0x10, 0x03,             // [0xac] call 3 (catch_from_caller) - returns 2
    0x6a,                   // [0xae] i32.add (1 + 2 = 3)
    0x02, 0x40,             // [0xaf] block $catch_handler (no result)
    0x1f, 0x7f,             // [0xb1] try_table (result i32)
    0x01,                   // 1 catch clause
    0x00, 0x00, 0x00,       // catch kind=0x00 (catch $exn), tag=0, label=0 ($catch_handler)
    0x10, 0x06,             // [0xb7] call 6 (middle_no_handler2) - throws from 3 levels deep
    0x0b,                   // [0xb9] end (try_table)
    0x41, 0x04,             // [0xba] i32.const 4 (normal path: great-grandparent would add 4)
    0x6a,                   // [0xbc] i32.add
    0x10, 0x05,             // [0xbd] call 5 (catch_from_grandparent)
    0x6a,                   // [0xbf] i32.add
    0x10, 0x07,             // [0xc0] call 7 (catch_all_in_callee)
    0x6a,                   // [0xc2] i32.add
    0x10, 0x08,             // [0xc3] call 8 (catch_all_from_caller)
    0x6a,                   // [0xc5] i32.add
    0x0f,                   // [0xc6] return (normal path)
    0x0b,                   // [0xc7] end (block $catch_handler)
    // Exception path: exception caught from middle_no_handler2
    0x1a,                   // [0xc8] drop (drop the partial result: 3)
    0x41, 0x04,             // [0xc9] i32.const 4
    0x41, 0x03,             // [0xcb] i32.const 3
    0x6a,                   // [0xcd] i32.add (4 + 3 = 7)
    0x10, 0x05,             // [0xce] call 5 (catch_from_grandparent) - returns 3
    0x6a,                   // [0xd0] i32.add (7 + 3 = 10)
    0x10, 0x07,             // [0xd1] call 7 (catch_all_in_callee) - returns 5
    0x6a,                   // [0xd3] i32.add (10 + 5 = 15)
    0x10, 0x08,             // [0xd4] call 8 (catch_all_from_caller) - returns 6
    0x6a,                   // [0xd6] i32.add (15 + 6 = 21)
    0x0b                    // [0xd7] end (function)
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
        if (result !== 21) {
            print("ERROR: Expected result=21, got result=", result);
            throw new Error("Test failed");
        }
    }
}
