// Operand stack depth for the call shapes that change stackFrameSize or numArguments, for
// qWasmStackValue. Every leaf has exactly one caller, so each breakpoint identifies one shape.
//
//   [0x88] leaf_r2     <- c_multires   kept 0x11111111; callee returns two values
//   [0xb8] leaf_p10    <- c_stackargs  kept 0x33333333 and passed ten arguments
//   [0xd2] leaf_ind    <- c_indirect   kept 0x55555555, then pushed the table index
//   [0xe2] leaf_empty  <- c_empty      kept nothing, so the caller's depth is 0
var wasm = new Uint8Array([
    0x00, 0x61, 0x73, 0x6d,                          // [0x00] magic
    0x01, 0x00, 0x00, 0x00,                          // [0x04] version

    // Type section
    0x01, 0x16,                                      // [0x08] section id=1, size=22
    0x03,                                            // [0x0a] 3 types
    0x60, 0x00, 0x00,                                // [0x0b] type 0: 0 params -> 0 results
    0x60, 0x00, 0x02, 0x7f, 0x7f,                    // [0x0e] type 1: 0 params -> 2 results
    0x60, 0x0a, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x00, // [0x13] type 2: 10 params -> 0 results

    // Function section
    0x03, 0x09,                                      // [0x20] section id=3, size=9
    0x08,                                            // [0x22] 8 functions
    0x00, 0x01, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00,  // [0x23] type index per function

    // Table section
    0x04, 0x04,                                      // [0x2b] section id=4, size=4
    0x01, 0x70, 0x00, 0x01,                          // [0x2d] 

    // Export section
    0x07, 0x33,                                      // [0x31] section id=7, size=51
    0x04,                                            // [0x33] 4 entries
    0x0a, 0x63, 0x5f, 0x6d, 0x75, 0x6c, 0x74, 0x69, 0x72, 0x65, 0x73, 0x00, 0x00, // [0x34] "c_multires" kind=0 index=0
    0x0b, 0x63, 0x5f, 0x73, 0x74, 0x61, 0x63, 0x6b, 0x61, 0x72, 0x67, 0x73, 0x00, 0x02, // [0x41] "c_stackargs" kind=0 index=2
    0x0a, 0x63, 0x5f, 0x69, 0x6e, 0x64, 0x69, 0x72, 0x65, 0x63, 0x74, 0x00, 0x04, // [0x4f] "c_indirect" kind=0 index=4
    0x07, 0x63, 0x5f, 0x65, 0x6d, 0x70, 0x74, 0x79, 0x00, 0x06, // [0x5c] "c_empty" kind=0 index=6

    // Element section
    0x09, 0x07,                                      // [0x66] section id=9, size=7
    0x01, 0x00, 0x41, 0x00, 0x0b, 0x01, 0x05,        // [0x68] 

    // Code section
    0x0a, 0x74,                                      // [0x6f] section id=10, size=116
    0x08,                                            // [0x71] 8 function bodies
    0x0d,                                            // [0x72] body size=13  func[0] <c_multires>
    0x00,                                            // [0x73] 0 local declaration groups
    0x41, 0x91, 0xa2, 0xc4, 0x88, 0x01,              // [0x74] i32.const 286331153
    0x10, 0x01,                                      // [0x7a] call 1
    0x1a,                                            // [0x7c] drop
    0x1a,                                            // [0x7d] drop
    0x1a,                                            // [0x7e] drop
    0x0b,                                            // [0x7f] end
    0x0f,                                            // [0x80] body size=15  func[1]
    0x00,                                            // [0x81] 0 local declaration groups
    0x41, 0xa1, 0xc2, 0x84, 0x89, 0x02,              // [0x82] i32.const 555819297
    0x01,                                            // [0x88] nop  <-- breakpoint
    0x41, 0xa2, 0xc4, 0x88, 0x91, 0x02,              // [0x89] i32.const 572662306
    0x0b,                                            // [0x8f] end
    0x1f,                                            // [0x90] body size=31  func[2] <c_stackargs>
    0x00,                                            // [0x91] 0 local declaration groups
    0x41, 0xb3, 0xe6, 0xcc, 0x99, 0x03,              // [0x92] i32.const 858993459
    0x41, 0x01,                                      // [0x98] i32.const 1
    0x41, 0x02,                                      // [0x9a] i32.const 2
    0x41, 0x03,                                      // [0x9c] i32.const 3
    0x41, 0x04,                                      // [0x9e] i32.const 4
    0x41, 0x05,                                      // [0xa0] i32.const 5
    0x41, 0x06,                                      // [0xa2] i32.const 6
    0x41, 0x07,                                      // [0xa4] i32.const 7
    0x41, 0x08,                                      // [0xa6] i32.const 8
    0x41, 0x09,                                      // [0xa8] i32.const 9
    0x41, 0x0a,                                      // [0xaa] i32.const 10
    0x10, 0x03,                                      // [0xac] call 3
    0x1a,                                            // [0xae] drop
    0x0b,                                            // [0xaf] end
    0x0a,                                            // [0xb0] body size=10  func[3]
    0x00,                                            // [0xb1] 0 local declaration groups
    0x41, 0xc4, 0x88, 0x91, 0xa2, 0x04,              // [0xb2] i32.const 1145324612
    0x01,                                            // [0xb8] nop  <-- breakpoint
    0x1a,                                            // [0xb9] drop
    0x0b,                                            // [0xba] end
    0x0e,                                            // [0xbb] body size=14  func[4] <c_indirect>
    0x00,                                            // [0xbc] 0 local declaration groups
    0x41, 0xd5, 0xaa, 0xd5, 0xaa, 0x05,              // [0xbd] i32.const 1431655765
    0x41, 0x00,                                      // [0xc3] i32.const 0
    0x11, 0x00, 0x00,                                // [0xc5] call_indirect 0 (type 0)
    0x1a,                                            // [0xc8] drop
    0x0b,                                            // [0xc9] end
    0x0a,                                            // [0xca] body size=10  func[5]
    0x00,                                            // [0xcb] 0 local declaration groups
    0x41, 0xe6, 0xcc, 0x99, 0xb3, 0x06,              // [0xcc] i32.const 1717986918
    0x01,                                            // [0xd2] nop  <-- breakpoint
    0x1a,                                            // [0xd3] drop
    0x0b,                                            // [0xd4] end
    0x04,                                            // [0xd5] body size=4  func[6] <c_empty>
    0x00,                                            // [0xd6] 0 local declaration groups
    0x10, 0x07,                                      // [0xd7] call 7
    0x0b,                                            // [0xd9] end
    0x0a,                                            // [0xda] body size=10  func[7]
    0x00,                                            // [0xdb] 0 local declaration groups
    0x41, 0xf7, 0xee, 0xdd, 0xbb, 0x07,              // [0xdc] i32.const 2004318071
    0x01,                                            // [0xe2] nop  <-- breakpoint
    0x1a,                                            // [0xe3] drop
    0x0b,                                            // [0xe4] end
]);

var module = new WebAssembly.Module(wasm);
var instance = new WebAssembly.Instance(module);

let shape = 0;
function driver() {
    // Rotate so every breakpoint is reachable from one running process.
    switch (shape++ % 4) {
    case 0: instance.exports.c_multires(); break;
    case 1: instance.exports.c_stackargs(); break;
    case 2: instance.exports.c_indirect(); break;
    default: instance.exports.c_empty(); break;
    }
}

print("DEBUGGER_READY");
let iteration = 0;
for (;;) {
    driver();
    iteration += 1;
    if (iteration % 1e6 == 0)
        print("iteration=", iteration);
}
