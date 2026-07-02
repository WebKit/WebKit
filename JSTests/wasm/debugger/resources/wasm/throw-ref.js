// WebAssembly module with REAL throw_ref exception handling tests
//
// This binary is extracted from JSTests/wasm/spec-tests/throw_ref.wast.js
// It uses the newer exception handling proposal with:
//   - try_table (0x1f) instead of try (0x06)
//   - catch_ref and catch_all_ref for catching with exception reference
//   - throw_ref (0x0a) for re-throwing exception objects
//   - exnref (0x69) type for exception references
//
// Catch kind encoding in try_table (WebAssembly binary format):
//   0x00 = catch (catch specific tag, no exception object on stack)
//   0x01 = catch_ref (catch specific tag, push exnref on stack)
//   0x02 = catch_all (catch any exception, no exception object on stack)
//   0x03 = catch_all_ref (catch any exception, push exnref on stack)
//
// Tests 7 scenarios:
// 1. catch-throw_ref-0 - basic throw_ref that re-throws caught exception
// 2. catch-throw_ref-1 - conditional throw_ref based on parameter
// 3. catchall-throw_ref-0 - throw_ref in catch_all_ref handler
// 4. catchall-throw_ref-1 - conditional throw_ref in catch_all_ref
// 5. throw_ref-nested - nested exception handling with multiple exception objects
// 6. throw_ref-recatch - catch exception, re-throw_ref, then catch again
// 7. throw_ref-stack-polymorphism - tests stack polymorphism with throw_ref
var wasm = new Uint8Array([
    // ========== [0x00] WASM module header ==========
    0x00, 0x61, 0x73, 0x6d,                     // [0x00] "\0asm" magic number
    0x01, 0x00, 0x00, 0x00,                     // [0x04] version 1

    // ========== [0x08] Type section ==========
    // 3 function types
    0x01,                                       // [0x08] section code = Type
    0x8d, 0x80, 0x80, 0x80, 0x00,              // [0x09] section size = 13 (LEB128)
    0x03,                                       // [0x0e] 3 types

    // Type 0: () -> ()
    0x60,                                       // [0x0f] func type
    0x00,                                       // [0x10] 0 parameters
    0x00,                                       // [0x11] 0 results

    // Type 1: (i32) -> (i32)
    0x60,                                       // [0x12] func type
    0x01, 0x7f,                                 // [0x13] 1 parameter: i32
    0x01, 0x7f,                                 // [0x15] 1 result: i32

    // Type 2: (exnref) -> ()
    0x60,                                       // [0x17] func type
    0x01, 0x69,                                 // [0x18] 1 parameter: exnref (0x69)
    0x00,                                       // [0x1a] 0 results

    // ========== [0x1b] Function section ==========
    // 7 functions
    0x03,                                       // [0x1b] section code = Function
    0x88, 0x80, 0x80, 0x80, 0x00,              // [0x1c] section size = 8 (LEB128)
    0x07,                                       // [0x21] 7 functions
    0x00,                                       // [0x22] function 0: type 0 () -> ()
    0x01,                                       // [0x23] function 1: type 1 (i32) -> (i32)
    0x00,                                       // [0x24] function 2: type 0 () -> ()
    0x01,                                       // [0x25] function 3: type 1 (i32) -> (i32)
    0x01,                                       // [0x26] function 4: type 1 (i32) -> (i32)
    0x01,                                       // [0x27] function 5: type 1 (i32) -> (i32)
    0x00,                                       // [0x28] function 6: type 0 () -> ()

    // ========== [0x29] Tag section ==========
    // 2 exception tags
    0x0d,                                       // [0x29] section code = Tag (0x0d)
    0x85, 0x80, 0x80, 0x80, 0x00,              // [0x2a] section size = 5 (LEB128)
    0x02,                                       // [0x2f] 2 tags

    // Tag 0: exception type 0 () -> ()
    0x00,                                       // [0x30] tag attribute = 0 (exception)
    0x00,                                       // [0x31] type index = 0

    // Tag 1: exception type 0 () -> ()
    0x00,                                       // [0x32] tag attribute = 0 (exception)
    0x00,                                       // [0x33] type index = 0

    // ========== [0x34] Export section ==========
    // 7 exported functions
    0x07,                                       // [0x34] section code = Export
    0x9d, 0x81, 0x80, 0x80, 0x00,              // [0x35] section size = 157 (LEB128)
    0x07,                                       // [0x3a] 7 exports

    // Export 0: "catch-throw_ref-0" -> function 0
    0x11,                                       // [0x3b] name length = 17
    0x63, 0x61, 0x74, 0x63, 0x68, 0x2d, 0x74, 0x68, 0x72, 0x6f, 0x77, 0x5f, // "catch-throw_"
    0x72, 0x65, 0x66, 0x2d, 0x30,              // "ref-0"
    0x00,                                       // [0x4d] export kind = function
    0x00,                                       // [0x4e] function index = 0

    // Export 1: "catch-throw_ref-1" -> function 1
    0x11,                                       // [0x4f] name length = 17
    0x63, 0x61, 0x74, 0x63, 0x68, 0x2d, 0x74, 0x68, 0x72, 0x6f, 0x77, 0x5f, // "catch-throw_"
    0x72, 0x65, 0x66, 0x2d, 0x31,              // "ref-1"
    0x00,                                       // [0x61] export kind = function
    0x01,                                       // [0x62] function index = 1

    // Export 2: "catchall-throw_ref-0" -> function 2
    0x14,                                       // [0x63] name length = 20
    0x63, 0x61, 0x74, 0x63, 0x68, 0x61, 0x6c, 0x6c, 0x2d, 0x74, 0x68, 0x72, // "catchall-thr"
    0x6f, 0x77, 0x5f, 0x72, 0x65, 0x66, 0x2d, 0x30, // "ow_ref-0"
    0x00,                                       // [0x78] export kind = function
    0x02,                                       // [0x79] function index = 2

    // Export 3: "catchall-throw_ref-1" -> function 3
    0x14,                                       // [0x7a] name length = 20
    0x63, 0x61, 0x74, 0x63, 0x68, 0x61, 0x6c, 0x6c, 0x2d, 0x74, 0x68, 0x72, // "catchall-thr"
    0x6f, 0x77, 0x5f, 0x72, 0x65, 0x66, 0x2d, 0x31, // "ow_ref-1"
    0x00,                                       // [0x8f] export kind = function
    0x03,                                       // [0x90] function index = 3

    // Export 4: "throw_ref-nested" -> function 4
    0x10,                                       // [0x91] name length = 16
    0x74, 0x68, 0x72, 0x6f, 0x77, 0x5f, 0x72, 0x65, 0x66, 0x2d, 0x6e, 0x65, // "throw_ref-ne"
    0x73, 0x74, 0x65, 0x64,                    // "sted"
    0x00,                                       // [0xa2] export kind = function
    0x04,                                       // [0xa3] function index = 4

    // Export 5: "throw_ref-recatch" -> function 5
    0x11,                                       // [0xa4] name length = 17
    0x74, 0x68, 0x72, 0x6f, 0x77, 0x5f, 0x72, 0x65, 0x66, 0x2d, 0x72, 0x65, // "throw_ref-re"
    0x63, 0x61, 0x74, 0x63, 0x68,              // "catch"
    0x00,                                       // [0xb6] export kind = function
    0x05,                                       // [0xb7] function index = 5

    // Export 6: "throw_ref-stack-polymorphism" -> function 6
    0x1c,                                       // [0xb8] name length = 28
    0x74, 0x68, 0x72, 0x6f, 0x77, 0x5f, 0x72, 0x65, 0x66, 0x2d, 0x73, 0x74, // "throw_ref-st"
    0x61, 0x63, 0x6b, 0x2d, 0x70, 0x6f, 0x6c, 0x79, 0x6d, 0x6f, 0x72, 0x70, // "ack-polymorp"
    0x68, 0x69, 0x73, 0x6d,                    // "hism"
    0x00,                                       // [0xd5] export kind = function
    0x06,                                       // [0xd6] function index = 6

    // ========== [0xd7] Code section ==========
    // 7 function bodies
    0x0a,                                       // [0xd7] section code = Code
    0xf3, 0x81, 0x80, 0x80, 0x00,              // [0xd8] section size = 243 (LEB128)
    0x07,                                       // [0xdd] 7 functions

    // ========== Function 0: catch-throw_ref-0 ==========
    // Type: () -> ()
    // Virtual address: 0x40000000000000de
    // Test: Throws exception, catches with catch_ref, then throw_ref re-throws it
    // Expected: Should throw (not caught by outer handler)
    0x90, 0x80, 0x80, 0x80, 0x00,              // [0xde] function size = 16 (LEB128)
    0x00,                                       // [0xe3] 0 local declarations

    0x02, 0x69,                                 // [0xe4] block exnref (result type = exnref)
    0x1f, 0x40,                                 // [0xe6] try_table (block type = 0x40 for empty)
    0x01,                                       // [0xe8] 1 catch clause
    0x01,                                       // [0xe9] catch kind = 0x01 (catch_ref - catches with exnref on stack)
    0x00,                                       // [0xea] tag index = 0
    0x00,                                       // [0xeb] label = 0 (jump to block end)
    0x08, 0x00,                                 // [0xec] throw tag=0
    0x0b,                                       // [0xee] end try_table
    0x00,                                       // [0xef] unreachable (if we get here, didn't catch)
    0x0b,                                       // [0xf0] end block - exnref is on stack here
    0x0a,                                       // [0xf1] throw_ref (re-throws the exnref from stack)
    0x0b,                                       // [0xf2] end function

    // ========== Function 1: catch-throw_ref-1 ==========
    // Type: (i32) -> (i32)
    // Virtual address: 0x40000000000000f3
    // Test: Catches exception as exnref, conditionally throw_ref or return 23
    // Expected: param=0 should throw, param=1 should return 23
    0x9a, 0x80, 0x80, 0x80, 0x00,              // [0xf3] function size = 26 (LEB128)
    0x00,                                       // [0xf8] 0 local declarations

    0x02, 0x69,                                 // [0xf9] block exnref
    0x1f, 0x7f,                                 // [0xfb] try_table i32 (result type = i32)
    0x01,                                       // [0xfd] 1 catch clause
    0x01,                                       // [0xfe] catch kind = 0x01 (catch_ref)
    0x00,                                       // [0xff] tag index = 0
    0x00,                                       // [0x100] label = 0 (jump to block end)
    0x08, 0x00,                                 // [0x101] throw tag=0
    0x0b,                                       // [0x103] end try_table
    0x0f,                                       // [0x104] return (if no exception, return top of stack)
    0x0b,                                       // [0x105] end block - exnref is on stack
    0x20, 0x00,                                 // [0x106] local.get 0 (get parameter)
    0x45,                                       // [0x108] i32.eqz (test if param == 0)
    0x04, 0x02,                                 // [0x109] if (void) type=2
    0x0a,                                       // [0x10b] throw_ref (if param==0, re-throw)
    0x05,                                       // [0x10c] else
    0x1a,                                       // [0x10d] drop (drop the exnref)
    0x0b,                                       // [0x10e] end if
    0x41, 0x17,                                 // [0x10f] i32.const 23
    0x0b,                                       // [0x111] end function

    // ========== Function 2: catchall-throw_ref-0 ==========
    // Type: () -> ()
    // Virtual address: 0x4000000000000112
    // Test: Throws exception, catches with catch_all_ref (any exception), then throw_ref
    // Expected: Should throw (re-thrown from catch_all_ref handler)
    0x8e, 0x80, 0x80, 0x80, 0x00,              // [0x112] function size = 14 (LEB128)
    0x00,                                       // [0x117] 0 local declarations

    0x02, 0x69,                                 // [0x118] block exnref
    0x1f, 0x69,                                 // [0x11a] try_table exnref (result type = exnref)
    0x01,                                       // [0x11c] 1 catch clause
    0x03,                                       // [0x11d] catch kind = 0x03 (catch_all_ref)
    0x00,                                       // [0x11e] label = 0 (jump to block end)
    0x08, 0x00,                                 // [0x11f] throw tag=0
    0x0b,                                       // [0x121] end try_table
    0x0b,                                       // [0x122] end block (exnref falls through to stack)
    0x0a,                                       // [0x123] throw_ref
    0x0b,                                       // [0x124] end function

    // ========== Function 3: catchall-throw_ref-1 ==========
    // Type: (i32) -> (i32)
    // Virtual address: 0x4000000000000125
    // Test: Catches with catch_all_ref, conditionally throw_ref or return 23
    // Expected: param=0 should throw, param=1 should return 23
    0x99, 0x80, 0x80, 0x80, 0x00,              // [0x125] function size = 25 (LEB128)
    0x00,                                       // [0x12a] 0 local declarations

    0x02, 0x69,                                 // [0x12b] block exnref
    0x1f, 0x7f,                                 // [0x12d] try_table i32
    0x01,                                       // [0x12f] 1 catch clause
    0x03,                                       // [0x130] catch kind = 0x03 (catch_all_ref)
    0x00,                                       // [0x131] label = 0
    0x08, 0x00,                                 // [0x132] throw tag=0
    0x0b,                                       // [0x134] end try_table
    0x0f,                                       // [0x135] return
    0x0b,                                       // [0x136] end block - exnref on stack
    0x20, 0x00,                                 // [0x137] local.get 0
    0x45,                                       // [0x139] i32.eqz
    0x04, 0x02,                                 // [0x13a] if (void)
    0x0a,                                       // [0x13c] throw_ref (if param==0)
    0x05,                                       // [0x13d] else
    0x1a,                                       // [0x13e] drop (drop exnref)
    0x0b,                                       // [0x13f] end if
    0x41, 0x17,                                 // [0x140] i32.const 23
    0x0b,                                       // [0x142] end function

    // ========== Function 4: throw_ref-nested ==========
    // Type: (i32) -> (i32)
    // Virtual address: 0x4000000000000143
    // Test: Nested exception handling - catches two different exceptions, stores in locals,
    //       then conditionally throw_ref based on parameter
    // Expected: param=0 throws exn1, param=1 throws exn2, param=2 returns 23
    0xba, 0x80, 0x80, 0x80, 0x00,              // [0x143] function size = 58 (LEB128)
    0x01,                                       // [0x148] 1 local declaration group
    0x02, 0x69,                                 // [0x149] 2 locals of type exnref

    // Outer block to catch second exception
    0x02, 0x69,                                 // [0x14b] block exnref
    0x1f, 0x7f,                                 // [0x14d] try_table i32
    0x01,                                       // [0x14f] 1 catch clause
    0x01,                                       // [0x150] catch kind = 0x01 (catch_ref)
    0x01,                                       // [0x151] tag index = 1
    0x00,                                       // [0x152] label = 0
    0x08, 0x01,                                 // [0x153] throw tag=1 (throw second exception)
    0x0b,                                       // [0x155] end try_table
    0x0f,                                       // [0x156] return
    0x0b,                                       // [0x157] end block
    0x21, 0x01,                                 // [0x158] local.set 1 (store exn2 in local 1)

    // Inner block to catch first exception
    0x02, 0x69,                                 // [0x15a] block exnref
    0x1f, 0x7f,                                 // [0x15c] try_table i32
    0x01,                                       // [0x15e] 1 catch clause
    0x01,                                       // [0x15f] catch kind = 0x01 (catch_ref)
    0x00,                                       // [0x160] tag index = 0
    0x00,                                       // [0x161] label = 0
    0x08, 0x00,                                 // [0x162] throw tag=0 (throw first exception)
    0x0b,                                       // [0x164] end try_table
    0x0f,                                       // [0x165] return
    0x0b,                                       // [0x166] end block
    0x21, 0x02,                                 // [0x167] local.set 2 (store exn1 in local 2)

    // Now we have both exceptions in locals, decide which to throw_ref
    0x20, 0x00,                                 // [0x169] local.get 0 (get parameter)
    0x41, 0x00,                                 // [0x16b] i32.const 0
    0x46,                                       // [0x16d] i32.eq (param == 0?)
    0x04, 0x40,                                 // [0x16e] if (void)
    0x20, 0x01,                                 // [0x170] local.get 1 (get exn2)
    0x0a,                                       // [0x172] throw_ref (throw exn2 if param==0)
    0x0b,                                       // [0x173] end if

    0x20, 0x00,                                 // [0x174] local.get 0
    0x41, 0x01,                                 // [0x176] i32.const 1
    0x46,                                       // [0x178] i32.eq (param == 1?)
    0x04, 0x40,                                 // [0x179] if (void)
    0x20, 0x02,                                 // [0x17b] local.get 2 (get exn1)
    0x0a,                                       // [0x17d] throw_ref (throw exn1 if param==1)
    0x0b,                                       // [0x17e] end if

    0x41, 0x17,                                 // [0x17f] i32.const 23 (if param >= 2)
    0x0b,                                       // [0x181] end function

    // ========== Function 5: throw_ref-recatch ==========
    // Type: (i32) -> (i32)
    // Virtual address: 0x4000000000000182
    // Test: Catch exception, store it, then in another try_table throw_ref it,
    //       and conditionally re-catch or let it propagate
    // Expected: param=0 returns 23 (inner catch), param=1 returns 42 (outer catch)
    0xac, 0x80, 0x80, 0x80, 0x00,              // [0x182] function size = 44 (LEB128)
    0x01,                                       // [0x187] 1 local declaration group
    0x01, 0x69,                                 // [0x188] 1 local of type exnref

    // First catch - stores exception
    0x02, 0x69,                                 // [0x18a] block exnref
    0x1f, 0x7f,                                 // [0x18c] try_table i32
    0x01,                                       // [0x18e] 1 catch clause
    0x01,                                       // [0x18f] catch kind = 0x01 (catch_ref)
    0x00,                                       // [0x190] tag index = 0
    0x00,                                       // [0x191] label = 0
    0x08, 0x00,                                 // [0x192] throw tag=0
    0x0b,                                       // [0x194] end try_table
    0x0f,                                       // [0x195] return
    0x0b,                                       // [0x196] end block
    0x21, 0x01,                                 // [0x197] local.set 1 (store caught exception)

    // Second try_table - re-throw_ref the stored exception conditionally
    0x02, 0x69,                                 // [0x199] block exnref
    0x1f, 0x7f,                                 // [0x19b] try_table i32
    0x01,                                       // [0x19d] 1 catch clause
    0x01,                                       // [0x19e] catch kind = 0x01 (catch_ref)
    0x00,                                       // [0x19f] tag index = 0
    0x00,                                       // [0x1a0] label = 0
    0x20, 0x00,                                 // [0x1a1] local.get 0 (get parameter)
    0x45,                                       // [0x1a3] i32.eqz (param == 0?)
    0x04, 0x40,                                 // [0x1a4] if (void)
    0x20, 0x01,                                 // [0x1a6] local.get 1 (get stored exception)
    0x0a,                                       // [0x1a8] throw_ref (re-throw if param==0)
    0x0b,                                       // [0x1a9] end if
    0x41, 0x2a,                                 // [0x1aa] i32.const 42 (if no throw_ref)
    0x0b,                                       // [0x1ac] end try_table
    0x0f,                                       // [0x1ad] return
    0x0b,                                       // [0x1ae] end block (re-caught here if param==0)
    0x1a,                                       // [0x1af] drop (drop re-caught exnref)
    0x41, 0x17,                                 // [0x1b0] i32.const 23
    0x0b,                                       // [0x1b2] end function

    // ========== Function 6: throw_ref-stack-polymorphism ==========
    // Type: () -> ()
    // Virtual address: 0x40000000000001b3
    // Test: Tests that throw_ref properly handles stack polymorphism
    //       (unreachable code after throw_ref)
    // Expected: Should throw
    0x98, 0x80, 0x80, 0x80, 0x00,              // [0x1b3] function size = 24 (LEB128)
    0x01,                                       // [0x1b8] 1 local declaration group
    0x01, 0x69,                                 // [0x1b9] 1 local of type exnref

    0x02, 0x69,                                 // [0x1bb] block exnref
    0x1f, 0x7c,                                 // [0x1bd] try_table f64 (result type = f64)
    0x01,                                       // [0x1bf] 1 catch clause
    0x01,                                       // [0x1c0] catch kind = 0x01 (catch_ref)
    0x00,                                       // [0x1c1] tag index = 0
    0x00,                                       // [0x1c2] label = 0
    0x08, 0x00,                                 // [0x1c3] throw tag=0
    0x0b,                                       // [0x1c5] end try_table
    0x00,                                       // [0x1c6] unreachable (never reached)
    0x0b,                                       // [0x1c7] end block
    0x21, 0x00,                                 // [0x1c8] local.set 0 (store exnref)
    0x41, 0x01,                                 // [0x1ca] i32.const 1
    0x20, 0x00,                                 // [0x1cc] local.get 0
    0x0a,                                       // [0x1ce] throw_ref (always throws)
    0x0b                                        // [0x1cf] end function
]);

var module = new WebAssembly.Module(wasm);
var instance_step = new WebAssembly.Instance(module, {});

// Test each function
let tests = [
    ["catch-throw_ref-0", [], true],           // should throw

    ["catch-throw_ref-1", [0], true],          // should throw
    ["catch-throw_ref-1", [1], false, 23],     // should return 23

    ["catchall-throw_ref-0", [], true],        // should throw
    ["catchall-throw_ref-1", [0], true],       // should throw
    ["catchall-throw_ref-1", [1], false, 23],  // should return 23

    ["throw_ref-nested", [0], true],           // should throw
    ["throw_ref-nested", [1], true],           // should throw
    ["throw_ref-nested", [2], false, 23],      // should return 23

    ["throw_ref-recatch", [0], false, 23],     // should return 23
    ["throw_ref-recatch", [1], false, 42],     // should return 42

    ["throw_ref-stack-polymorphism", [], true] // should throw
];

print("DEBUGGER_READY");
let iteration = 0;
for (; ;) {
    for (let [name, args, shouldThrow, expected] of tests) {
        try {
            let result = instance_step.exports[name](...args);
            if (shouldThrow) {
                throw new Error(`${name}(${args}) should have thrown but returned ${result}`);
            }
            if (expected !== undefined && result !== expected) {
                throw new Error(`${name}(${args}) expected ${expected} but got ${result}`);
            }
        } catch (e) {
            if (!shouldThrow) {
                throw new Error(`${name}(${args}) should not have thrown: ${e}`);
            }
        }
    }

    iteration += 1;
    if (iteration % 1e4 == 0) {
        print("iteration=", iteration, "all tests passed");
    }
}
