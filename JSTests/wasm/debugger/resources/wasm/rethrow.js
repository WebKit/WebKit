// WebAssembly module with comprehensive rethrow exception handling tests
// Tests various rethrow scenarios:
// 1. Basic rethrow in catch handler
// 2. Rethrow in catch_all handler
// 3. Rethrow after partial handling
// 4. Rethrow across function boundaries (to caller)
// 5. Rethrow chains (multiple rethrows through nested handlers)
// 6. Mixed rethrow with catch and catch_all handlers
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
    0x01,                   // function 0 (basic_rethrow_in_catch): type 1
    0x01,                   // function 1 (rethrow_in_catch_all): type 1
    0x01,                   // function 2 (rethrow_after_partial_handling): type 1
    0x01,                   // function 3 (rethrows_to_caller): type 1
    0x01,                   // function 4 (catches_rethrown_from_callee): type 1
    0x01,                   // function 5 (rethrow_chain): type 1
    0x01,                   // function 6 (mixed_rethrow_with_catch_all): type 1
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
    0x97, 0x01,             // section size=151
    0x08,                   // 8 functions

    // [0x34] Function 0: basic_rethrow_in_catch - catch and immediately rethrow
    // Virtual address base: 0x4000000000000035
    0x12,                   // body size=18
    0x00,                   // 0 locals
    0x06, 0x7f,             // [0x36] try (result i32) - outer
    0x06, 0x7f,             // [0x38] try (result i32) - inner
    0x08, 0x00,             // [0x3a] throw tag=0
    0x07, 0x00,             // [0x3c] catch tag=0 (inner catch)
    0x09, 0x00,             // [0x3e] rethrow 0 (rethrow to outer try)
    0x0b,                   // [0x40] end (inner try)
    0x07, 0x00,             // [0x41] catch tag=0 (outer catch)
    0x41, 0x01,             // [0x43] i32.const 1
    0x0b,                   // [0x45] end (outer try)
    0x0b,                   // [0x46] end (function)

    // [0x47] Function 1: rethrow_in_catch_all - catch with catch_all and rethrow
    // Virtual address base: 0x4000000000000048
    0x11,                   // body size=17
    0x00,                   // 0 locals
    0x06, 0x7f,             // [0x49] try (result i32) - outer
    0x06, 0x7f,             // [0x4b] try (result i32) - inner
    0x08, 0x00,             // [0x4d] throw tag=0
    0x19,                   // [0x4f] catch_all (inner catch_all)
    0x09, 0x00,             // [0x50] rethrow 0 (rethrow unknown exception to outer)
    0x0b,                   // [0x52] end (inner try)
    0x07, 0x00,             // [0x53] catch tag=0 (outer catch)
    0x41, 0x02,             // [0x55] i32.const 2
    0x0b,                   // [0x57] end (outer try)
    0x0b,                   // [0x58] end (function)

    // [0x59] Function 2: rethrow_after_partial_handling - do work before rethrowing
    // Virtual address base: 0x400000000000005a
    0x12,                   // body size=18
    0x00,                   // 0 locals
    0x06, 0x7f,             // [0x5b] try (result i32) - outer
    0x06, 0x7f,             // [0x5d] try (result i32) - inner
    0x08, 0x00,             // [0x5f] throw tag=0
    0x07, 0x00,             // [0x61] catch tag=0 (inner catch - partial handling)
    0x09, 0x00,             // [0x63] rethrow 0 (rethrow after handling)
    0x0b,                   // [0x65] end (inner try)
    0x07, 0x00,             // [0x66] catch tag=0 (outer catch)
    0x41, 0x03,             // [0x68] i32.const 3
    0x0b,                   // [0x6a] end (outer try)
    0x0b,                   // [0x6b] end (function)

    // [0x6c] Function 3: rethrows_to_caller - function that catches and rethrows to caller
    // Virtual address base: 0x400000000000006d
    0x0b,                   // body size=11
    0x00,                   // 0 locals
    0x06, 0x7f,             // [0x6e] try (result i32)
    0x08, 0x00,             // [0x70] throw tag=0
    0x07, 0x00,             // [0x72] catch tag=0
    0x09, 0x00,             // [0x74] rethrow 0 (rethrow to caller)
    0x0b,                   // [0x76] end (try)
    0x0b,                   // [0x77] end (function)

    // [0x78] Function 4: catches_rethrown_from_callee - catches exception rethrown by function 3
    // Virtual address base: 0x4000000000000079
    0x0b,                   // body size=11
    0x00,                   // 0 locals
    0x06, 0x7f,             // [0x7a] try (result i32)
    0x10, 0x03,             // [0x7c] call function=3 (rethrows_to_caller)
    0x07, 0x00,             // [0x7e] catch tag=0
    0x41, 0x04,             // [0x80] i32.const 4
    0x0b,                   // [0x82] end (try)
    0x0b,                   // [0x83] end (function)

    // [0x84] Function 5: rethrow_chain - multiple rethrows through nested handlers
    // Virtual address base: 0x4000000000000085
    0x19,                   // body size=25
    0x00,                   // 0 locals
    0x06, 0x7f,             // [0x86] try (result i32) - outer
    0x06, 0x7f,             // [0x88] try (result i32) - middle
    0x06, 0x7f,             // [0x8a] try (result i32) - inner
    0x08, 0x00,             // [0x8c] throw tag=0
    0x07, 0x00,             // [0x8e] catch tag=0 (inner catch)
    0x09, 0x00,             // [0x90] rethrow 0 (inner rethrows to middle)
    0x0b,                   // [0x92] end (inner try)
    0x07, 0x00,             // [0x93] catch tag=0 (middle catch)
    0x09, 0x00,             // [0x95] rethrow 0 (middle rethrows to outer)
    0x0b,                   // [0x97] end (middle try)
    0x07, 0x00,             // [0x98] catch tag=0 (outer catch)
    0x41, 0x05,             // [0x9a] i32.const 5
    0x0b,                   // [0x9c] end (outer try)
    0x0b,                   // [0x9d] end (function)

    // [0x9e] Function 6: mixed_rethrow_with_catch_all - mix of catch and catch_all with rethrow
    // Virtual address base: 0x400000000000009f
    0x17,                   // body size=23
    0x00,                   // 0 locals
    0x06, 0x7f,             // [0xa0] try (result i32) - outer
    0x06, 0x7f,             // [0xa2] try (result i32) - middle
    0x06, 0x7f,             // [0xa4] try (result i32) - inner
    0x08, 0x00,             // [0xa6] throw tag=0
    0x19,                   // [0xa8] catch_all (inner catch_all)
    0x09, 0x00,             // [0xa9] rethrow 0 (rethrow from catch_all to middle catch)
    0x0b,                   // [0xab] end (inner try)
    0x07, 0x00,             // [0xac] catch tag=0 (middle catch)
    0x09, 0x00,             // [0xae] rethrow 0 (rethrow from catch to outer catch_all)
    0x0b,                   // [0xb0] end (middle try)
    0x19,                   // [0xb1] catch_all (outer catch_all)
    0x41, 0x06,             // [0xb2] i32.const 6
    0x0b,                   // [0xb4] end (outer try)
    0x0b,                   // [0xb5] end (function)

    // [0xb6] Function 7: test_all - orchestrates all rethrow scenarios
    // Virtual address base: 0x40000000000000b7
    0x13,                   // body size=19
    0x00,                   // 0 locals
    0x10, 0x00,             // [0xb8] call function=0 (basic_rethrow_in_catch) → returns 1
    0x10, 0x01,             // [0xba] call function=1 (rethrow_in_catch_all) → returns 2
    0x6a,                   // [0xbc] i32.add (1 + 2 = 3)
    0x10, 0x02,             // [0xbd] call function=2 (rethrow_after_partial_handling) → returns 3
    0x6a,                   // [0xbf] i32.add (3 + 3 = 6)
    0x10, 0x04,             // [0xc0] call function=4 (catches_rethrown_from_callee) → returns 4
    0x6a,                   // [0xc2] i32.add (6 + 4 = 10)
    0x10, 0x05,             // [0xc3] call function=5 (rethrow_chain) → returns 5
    0x6a,                   // [0xc5] i32.add (10 + 5 = 15)
    0x10, 0x06,             // [0xc6] call function=6 (mixed_rethrow_with_catch_all) → returns 6
    0x6a,                   // [0xc8] i32.add (15 + 6 = 21)
    0x0b,                   // [0xc9] end (function)
    // [0xca] End of code section
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
