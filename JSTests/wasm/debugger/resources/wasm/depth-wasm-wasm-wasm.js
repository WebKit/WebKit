// Operand stack depth across a pure WASM -> WASM -> WASM call chain, for qWasmStackValue.
// Each frame holds a different number of entries at the breakpoint, and the middle call passes an
// argument, so a caller's depth must include the entry that call consumed.
//
// Call chain: JS main -> test[W] -> middle[W] -> inner[W]
//
//   frame #0  inner   depth 2   index 0 = 0x55555555, index 1 = 0x66666666
//   frame #1  middle  depth 1   index 0 = 0x44444444        (callee takes no arguments)
//   frame #2  test    depth 3   index 0 = 0x11111111, index 1 = 0x22222222,
//                               index 2 = 0x33333333        (consumed as middle's argument)
//
// Built from depth-wasm-wasm-wasm.wat with wat2wasm; the breakpoint is the nop at file offset 0x60.
var wasm = new Uint8Array([
    0x00, 0x61, 0x73, 0x6d,                          // [0x00] magic
    0x01, 0x00, 0x00, 0x00,                          // [0x04] version

    // Type section
    0x01, 0x08,                                      // [0x08] section id=1, size=8
    0x02,                                            // [0x0a] 2 types
    0x60, 0x00, 0x00,                                // [0x0b] type 0: 0 params -> 0 results
    0x60, 0x01, 0x7f, 0x00,                          // [0x0e] type 1: 1 params -> 0 results

    // Function section
    0x03, 0x04,                                      // [0x12] section id=3, size=4
    0x03,                                            // [0x14] 3 functions
    0x00, 0x01, 0x00,                                // [0x15] type index per function

    // Export section
    0x07, 0x10,                                      // [0x18] section id=7, size=16
    0x02,                                            // [0x1a] 2 entries
    0x04, 0x74, 0x65, 0x73, 0x74, 0x00, 0x00,        // [0x1b] "test" kind=0 index=0
    0x05, 0x69, 0x6e, 0x6e, 0x65, 0x72, 0x00, 0x02,  // [0x22] "inner" kind=0 index=2

    // Code section
    0x0a, 0x38,                                      // [0x2a] section id=10, size=56
    0x03,                                            // [0x2c] 3 function bodies
    0x18,                                            // [0x2d] body size=24  func[0] <test>
    0x00,                                            // [0x2e] 0 local declaration groups
    0x41, 0x91, 0xa2, 0xc4, 0x88, 0x01,              // [0x2f] i32.const 286331153
    0x41, 0xa2, 0xc4, 0x88, 0x91, 0x02,              // [0x35] i32.const 572662306
    0x41, 0xb3, 0xe6, 0xcc, 0x99, 0x03,              // [0x3b] i32.const 858993459
    0x10, 0x01,                                      // [0x41] call 1
    0x1a,                                            // [0x43] drop
    0x1a,                                            // [0x44] drop
    0x0b,                                            // [0x45] end
    0x0b,                                            // [0x46] body size=11  func[1]
    0x00,                                            // [0x47] 0 local declaration groups
    0x41, 0xc4, 0x88, 0x91, 0xa2, 0x04,              // [0x48] i32.const 1145324612
    0x10, 0x02,                                      // [0x4e] call 2 <inner>
    0x1a,                                            // [0x50] drop
    0x0b,                                            // [0x51] end
    0x11,                                            // [0x52] body size=17  func[2] <inner>
    0x00,                                            // [0x53] 0 local declaration groups
    0x41, 0xd5, 0xaa, 0xd5, 0xaa, 0x05,              // [0x54] i32.const 1431655765
    0x41, 0xe6, 0xcc, 0x99, 0xb3, 0x06,              // [0x5a] i32.const 1717986918
    0x01,                                            // [0x60] nop  <-- breakpoint
    0x1a,                                            // [0x61] drop
    0x1a,                                            // [0x62] drop
    0x0b,                                            // [0x63] end
]);

var module = new WebAssembly.Module(wasm);
var instance = new WebAssembly.Instance(module);

let test = instance.exports.test;

print("DEBUGGER_READY");
let iteration = 0;
for (;;) {
    test();
    iteration += 1;
    if (iteration % 1e6 == 0)
        print("iteration=", iteration);
}

// b 0x4000000000000060
