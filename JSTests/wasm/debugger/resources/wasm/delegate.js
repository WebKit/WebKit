// WebAssembly module with comprehensive try-delegate exception handling tests
// Tests various delegate scenarios:
// 1. Delegate to immediate parent (depth 0)
// 2. Delegate to grandparent (depth 1)
// 3. Delegate to great-grandparent (depth 2)
// 4. Delegate to caller (function boundary)
// 5. Delegate chains (multiple delegates)
// 6. Delegate past catch_all handlers

var wasm = new Uint8Array([
    // [0x00] WASM header
    0x00, 0x61, 0x73, 0x6d, // magic
    0x01, 0x00, 0x00, 0x00, // version

    // [0x08] Type section: 2 function types
    0x01, 0x08,             // section id=1, size=8
    0x02,                   // 2 types
    0x60, 0x00, 0x00,       // Type 0: (func [] -> []) - void function for exception tag
    0x60, 0x00, 0x01, 0x7f, // Type 1: (func [] -> [i32]) - function returning i32

    // [0x12] Function section: 9 functions
    0x03, 0x0a,             // section id=3, size=10
    0x09,                   // 9 functions
    0x01,                   // function 0 (thrower): type 1
    0x01,                   // function 1 (delegate_to_parent): type 1
    0x01,                   // function 2 (delegate_to_grandparent): type 1
    0x01,                   // function 3 (delegate_to_great_grandparent): type 1
    0x01,                   // function 4 (no_handler_delegates_to_caller): type 1
    0x01,                   // function 5 (catches_delegated_from_callee): type 1
    0x01,                   // function 6 (delegate_chain): type 1
    0x01,                   // function 7 (delegate_past_catch_all): type 1
    0x01,                   // function 8 (test_all): type 1

    // [0x1e] Tag section: 1 exception tag
    0x0d, 0x03,             // section id=13, size=3
    0x01,                   // 1 tag
    0x00, 0x00,             // attribute=0 (exception), type index 0 (void)

    // [0x23] Export section: export function 8 as "test_all"
    0x07, 0x0c,             // section id=7, size=12
    0x01,                   // 1 export
    0x08,                   // name length=8
    0x74, 0x65, 0x73, 0x74, 0x5f, 0x61, 0x6c, 0x6c, // "test_all"
    0x00, 0x08,             // export kind=function, function index=8

    // [0x31] Code section
    0x0a,                   // section id=10
    0xa4, 0x01,             // section size=164
    0x09,                   // 9 functions

    // [0x35] Function 0: thrower - throws exception
    // Virtual address base: 0x4000000000000036
    0x06,                   // body size=6
    0x00,                   // 0 locals
    0x08, 0x00,             // [0x37] throw tag=0
    0x41, 0x00,             // [0x39] i32.const 0 (unreachable)
    0x0b,                   // [0x3b] end

    // [0x3c] Function 1: delegate_to_parent - delegates to immediate parent (depth 0)
    // Virtual address base: 0x400000000000003d
    0x0f,                   // body size=15
    0x00,                   // 0 locals
    0x06, 0x7f,             // [0x3e] try (result i32) - outer
    0x06, 0x7f,             // [0x40] try (result i32) - inner
    0x08, 0x00,             // [0x42] throw tag=0
    0x18, 0x00,             // [0x44] delegate 0 (to outer try at 0x46)
    0x07, 0x00,             // [0x46] catch tag=0 (outer catch)
    0x41, 0x01,             // [0x48] i32.const 1
    0x0b,                   // [0x4a] end (outer try)
    0x0b,                   // [0x4b] end (function)

    // [0x4c] Function 2: delegate_to_grandparent - delegates to grandparent (depth 1)
    // Virtual address base: 0x400000000000004d
    0x17,                   // body size=23
    0x00,                   // 0 locals
    0x06, 0x7f,             // [0x4e] try (result i32) - grandparent
    0x06, 0x7f,             // [0x50] try (result i32) - parent
    0x06, 0x7f,             // [0x52] try (result i32) - inner
    0x08, 0x00,             // [0x54] throw tag=0
    0x18, 0x01,             // [0x56] delegate 1 (skip parent at 0x58, delegate to grandparent at 0x5e)
    0x07, 0x00,             // [0x58] catch tag=0 (parent catch - skipped)
    0x41, 0xe1, 0x00,       // [0x5a] i32.const 97
    0x0b,                   // [0x5d] end (parent try)
    0x07, 0x00,             // [0x5e] catch tag=0 (grandparent catch)
    0x41, 0x02,             // [0x60] i32.const 2
    0x0b,                   // [0x62] end (grandparent try)
    0x0b,                   // [0x63] end (function)

    // [0x64] Function 3: delegate_to_great_grandparent - delegates 2 levels up (depth 2)
    // Virtual address base: 0x4000000000000065
    0x1f,                   // body size=31
    0x00,                   // 0 locals
    0x06, 0x7f,             // [0x66] try (result i32) - great-grandparent
    0x06, 0x7f,             // [0x68] try (result i32) - grandparent
    0x06, 0x7f,             // [0x6a] try (result i32) - parent
    0x06, 0x7f,             // [0x6c] try (result i32) - inner
    0x08, 0x00,             // [0x6e] throw tag=0
    0x18, 0x02,             // [0x70] delegate 2 (skip 2 levels, delegate to great-grandparent at 0x7e)
    0x07, 0x00,             // [0x72] catch tag=0 (parent catch - skipped)
    0x41, 0xde, 0x00,       // [0x74] i32.const 94
    0x0b,                   // [0x77] end (parent try)
    0x07, 0x00,             // [0x78] catch tag=0 (grandparent catch - skipped)
    0x41, 0xdc, 0x00,       // [0x7a] i32.const 92
    0x0b,                   // [0x7d] end (grandparent try)
    0x07, 0x00,             // [0x7e] catch tag=0 (great-grandparent catch)
    0x41, 0x03,             // [0x80] i32.const 3
    0x0b,                   // [0x82] end (great-grandparent try)
    0x0b,                   // [0x83] end (function)

    // [0x84] Function 4: no_handler_delegates_to_caller - delegate to function boundary
    // Virtual address base: 0x4000000000000085
    0x08,                   // body size=8
    0x00,                   // 0 locals
    0x06, 0x7f,             // [0x86] try (result i32)
    0x08, 0x00,             // [0x88] throw tag=0
    0x18, 0x00,             // [0x8a] delegate 0 (to function boundary - propagates to caller)
    0x0b,                   // [0x8c] end

    // [0x8d] Function 5: catches_delegated_from_callee
    // Virtual address base: 0x400000000000008e
    0x0b,                   // body size=11
    0x00,                   // 0 locals
    0x06, 0x7f,             // [0x8f] try (result i32)
    0x10, 0x04,             // [0x91] call function=4 (no_handler_delegates_to_caller)
    0x07, 0x00,             // [0x93] catch tag=0
    0x41, 0x04,             // [0x95] i32.const 4
    0x0b,                   // [0x97] end (try)
    0x0b,                   // [0x98] end (function)

    // [0x99] Function 6: delegate_chain - multiple delegates in chain
    // Virtual address base: 0x400000000000009a
    0x13,                   // body size=19
    0x00,                   // 0 locals
    0x06, 0x7f,             // [0x9b] try (result i32) - outer
    0x06, 0x7f,             // [0x9d] try (result i32) - middle
    0x06, 0x7f,             // [0x9f] try (result i32) - inner
    0x08, 0x00,             // [0xa1] throw tag=0
    0x18, 0x00,             // [0xa3] delegate 0 (to middle try at 0x9d)
    0x18, 0x00,             // [0xa5] delegate 0 (middle delegates to outer try at 0x9b)
    0x07, 0x00,             // [0xa7] catch tag=0 (outer catch)
    0x41, 0x05,             // [0xa9] i32.const 5
    0x0b,                   // [0xab] end (outer try)
    0x0b,                   // [0xac] end (function)

    // [0xad] Function 7: delegate_past_catch_all - delegate skips catch_all handlers
    // Virtual address base: 0x40000000000000ae
    0x16,                   // body size=22
    0x00,                   // 0 locals
    0x06, 0x7f,             // [0xaf] try (result i32) - grandparent
    0x06, 0x7f,             // [0xb1] try (result i32) - parent
    0x06, 0x7f,             // [0xb3] try (result i32) - inner
    0x08, 0x00,             // [0xb5] throw tag=0
    0x18, 0x01,             // [0xb7] delegate 1 (skip parent's catch_all, delegate to grandparent)
    0x19,                   // [0xb9] catch_all (parent catch_all - skipped by delegate 1)
    0x41, 0xd6, 0x00,       // [0xba] i32.const 86
    0x0b,                   // [0xbd] end (parent try)
    0x07, 0x00,             // [0xbe] catch tag=0 (grandparent catch)
    0x41, 0x06,             // [0xc0] i32.const 6
    0x0b,                   // [0xc2] end (grandparent try)
    0x0b,                   // [0xc3] end (function)

    // [0xc4] Function 8: test_all - orchestrates all delegate scenarios
    // Virtual address base: 0x40000000000000c5
    0x13,                   // body size=19
    0x00,                   // 0 locals
    0x10, 0x01,             // [0xc6] call function=1 (delegate_to_parent) → returns 1
    0x10, 0x02,             // [0xc8] call function=2 (delegate_to_grandparent) → returns 2
    0x6a,                   // [0xca] i32.add (1 + 2 = 3)
    0x10, 0x03,             // [0xcb] call function=3 (delegate_to_great_grandparent) → returns 3
    0x6a,                   // [0xcd] i32.add (3 + 3 = 6)
    0x10, 0x05,             // [0xce] call function=5 (catches_delegated_from_callee) → returns 4
    0x6a,                   // [0xd0] i32.add (6 + 4 = 10)
    0x10, 0x06,             // [0xd1] call function=6 (delegate_chain) → returns 5
    0x6a,                   // [0xd3] i32.add (10 + 5 = 15)
    0x10, 0x07,             // [0xd4] call function=7 (delegate_past_catch_all) → returns 6
    0x6a,                   // [0xd6] i32.add (15 + 6 = 21)
    0x0b,                   // [0xd7] end (function)
    // [0xd8] End of code section
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
