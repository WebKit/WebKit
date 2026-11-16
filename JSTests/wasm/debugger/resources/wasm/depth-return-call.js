// Operand stack depth when a frame in the chain tail-called: its frame was replaced by the tail
// callee's, whose signature is NOT the one the surviving caller called with.
//
//   [0x77] leaf_a  <- mid_a (2 params) tail-called it (0 params); frame #1 is outer_a, depth 3
//   [0x9f] leaf_b  <- mid_b (0 params) tail-called it (2 params); frame #1 is outer_b, depth 1
//   [0xca] leaf_c  <- reached the same way through call_indirect, so the in-flight call's metadata
//                     is CallIndirectMetadata; frame #1 is outer_c, depth 3
var wasm = new Uint8Array([
    0x00, 0x61, 0x73, 0x6d,                                                  // [0x00] magic
    0x01, 0x00, 0x00, 0x00,                                                  // [0x04] version

    // Type section
    0x01, 0x09,                                                              // [0x08] section id=1, size=9
    0x02, 0x60, 0x00, 0x00, 0x60, 0x02, 0x7f, 0x7f, 0x00,                    // [0x0a]

    // Function section
    0x03, 0x0a,                                                              // [0x13] section id=3, size=10
    0x09, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00,              // [0x15]

    // Table section
    0x04, 0x04,                                                              // [0x1f] section id=4, size=4
    0x01, 0x70, 0x00, 0x01,                                                  // [0x21]

    // Export section
    0x07, 0x1f,                                                              // [0x25] section id=7, size=31
    0x03, 0x07, 0x6f, 0x75, 0x74, 0x65, 0x72, 0x5f, 0x61, 0x00, 0x00, 0x07,  // [0x27]
    0x6f, 0x75, 0x74, 0x65, 0x72, 0x5f, 0x62, 0x00, 0x03, 0x07, 0x6f, 0x75,  // [0x33]
    0x74, 0x65, 0x72, 0x5f, 0x63, 0x00, 0x06,                                // [0x3f]

    // Element section
    0x09, 0x07,                                                              // [0x46] section id=9, size=7
    0x01, 0x00, 0x41, 0x00, 0x0b, 0x01, 0x07,                                // [0x48]

    // Code section
    0x0a, 0x7c,                                                              // [0x4f] section id=10, size=124
    0x09,                                                                    // [0x51] 9 function bodies
    0x17,                                                                    // [0x52] body size=23  func[0] <outer_a>
    0x00,                                                                    // [0x53] 0 local declaration groups
    0x41, 0x91, 0xa2, 0xc4, 0x88, 0x01,                                      // [0x54] i32.const 286331153
    0x41, 0xa2, 0xc4, 0x88, 0x91, 0x02,                                      // [0x5a] i32.const 572662306
    0x41, 0xb3, 0xe6, 0xcc, 0x99, 0x03,                                      // [0x60] i32.const 858993459
    0x10, 0x01,                                                              // [0x66] call 1
    0x1a,                                                                    // [0x68] drop
    0x0b,                                                                    // [0x69] end
    0x04,                                                                    // [0x6a] body size=4  func[1]
    0x00,                                                                    // [0x6b] 0 local declaration groups
    0x12, 0x02,                                                              // [0x6c] return_call 2
    0x0b,                                                                    // [0x6e] end
    0x0a,                                                                    // [0x6f] body size=10  func[2]
    0x00,                                                                    // [0x70] 0 local declaration groups
    0x41, 0xc4, 0x88, 0x91, 0xa2, 0x04,                                      // [0x71] i32.const 1145324612
    0x01,                                                                    // [0x77] nop  <-- breakpoint
    0x1a,                                                                    // [0x78] drop
    0x0b,                                                                    // [0x79] end
    0x0b,                                                                    // [0x7a] body size=11  func[3] <outer_b>
    0x00,                                                                    // [0x7b] 0 local declaration groups
    0x41, 0xd5, 0xaa, 0xd5, 0xaa, 0x05,                                      // [0x7c] i32.const 1431655765
    0x10, 0x04,                                                              // [0x82] call 4
    0x1a,                                                                    // [0x84] drop
    0x0b,                                                                    // [0x85] end
    0x10,                                                                    // [0x86] body size=16  func[4]
    0x00,                                                                    // [0x87] 0 local declaration groups
    0x41, 0xe6, 0xcc, 0x99, 0xb3, 0x06,                                      // [0x88] i32.const 1717986918
    0x41, 0xf7, 0xee, 0xdd, 0xbb, 0x07,                                      // [0x8e] i32.const 2004318071
    0x12, 0x05,                                                              // [0x94] return_call 5
    0x0b,                                                                    // [0x96] end
    0x0a,                                                                    // [0x97] body size=10  func[5]
    0x00,                                                                    // [0x98] 0 local declaration groups
    0x41, 0x88, 0x90, 0xa0, 0xc0, 0x00,                                      // [0x99] i32.const 134744072
    0x01,                                                                    // [0x9f] nop  <-- breakpoint
    0x1a,                                                                    // [0xa0] drop
    0x0b,                                                                    // [0xa1] end
    0x1a,                                                                    // [0xa2] body size=26  func[6] <outer_c>
    0x00,                                                                    // [0xa3] 0 local declaration groups
    0x41, 0x99, 0xb3, 0xe6, 0xcc, 0x79,                                      // [0xa4] i32.const 2576980377
    0x41, 0xaa, 0xd5, 0xaa, 0xd5, 0x7a,                                      // [0xaa] i32.const 2863311530
    0x41, 0xbb, 0xf7, 0xee, 0xdd, 0x7b,                                      // [0xb0] i32.const 3149642683
    0x41, 0x00,                                                              // [0xb6] i32.const 0
    0x11, 0x01, 0x00,                                                        // [0xb8] call_indirect 0 (type 1)
    0x1a,                                                                    // [0xbb] drop
    0x0b,                                                                    // [0xbc] end
    0x04,                                                                    // [0xbd] body size=4  func[7]
    0x00,                                                                    // [0xbe] 0 local declaration groups
    0x12, 0x08,                                                              // [0xbf] return_call 8
    0x0b,                                                                    // [0xc1] end
    0x0a,                                                                    // [0xc2] body size=10  func[8]
    0x00,                                                                    // [0xc3] 0 local declaration groups
    0x41, 0xcc, 0x99, 0xb3, 0xe6, 0x7c,                                      // [0xc4] i32.const 3435973836
    0x01,                                                                    // [0xca] nop  <-- breakpoint
    0x1a,                                                                    // [0xcb] drop
    0x0b,                                                                    // [0xcc] end
]);

var module = new WebAssembly.Module(wasm);
var instance = new WebAssembly.Instance(module);

let shape = 0;
function driver() {
    switch (shape++ % 3) {
    case 0: instance.exports.outer_a(); break;
    case 1: instance.exports.outer_b(); break;
    default: instance.exports.outer_c(); break;
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
