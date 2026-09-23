// Operand stack depth across WASM -> WASM -> JS -> WASM -> WASM, for qWasmStackValue.
// A wasm->wasm hop on each side of a JS boundary, so both anchor kinds appear in one stack.
//
// Call chain: JS main -> test_a[W] -> test_b[W] -> js.mid_callback -> test_c[W] -> wasm_leaf[W]
//
//   frame #0  wasm_leaf  depth 2   index 0 = 0x55555555, index 1 = 0x66666666
//   frame #1  test_c     depth 1   index 0 = 0x44444444
//   frame #3  test_b     depth 2   index 0 = 0x22222222, index 1 = 0x33333333
//   frame #4  test_a     depth 1   index 0 = 0x11111111
//
var wasm = new Uint8Array([
    0x00, 0x61, 0x73, 0x6d,                          // [0x00] magic
    0x01, 0x00, 0x00, 0x00,                          // [0x04] version

    // Type section
    0x01, 0x04,                                      // [0x08] section id=1, size=4
    0x01,                                            // [0x0a] 1 types
    0x60, 0x00, 0x00,                                // [0x0b] type 0: 0 params -> 0 results

    // Import section
    0x02, 0x13,                                      // [0x0e] section id=2, size=19
    0x01,                                            // [0x10] 1 entries
    0x02, 0x6a, 0x73, 0x0c, 0x6d, 0x69, 0x64, 0x5f, 0x63, 0x61, 0x6c, 0x6c, 0x62, 0x61, 0x63, 0x6b, 0x00, 0x00, // [0x11] "js.mid_callback" kind=0 index=0

    // Function section
    0x03, 0x05,                                      // [0x23] section id=3, size=5
    0x04,                                            // [0x25] 4 functions
    0x00, 0x00, 0x00, 0x00,                          // [0x26] type index per function

    // Export section
    0x07, 0x1f,                                      // [0x2a] section id=7, size=31
    0x03,                                            // [0x2c] 3 entries
    0x06, 0x74, 0x65, 0x73, 0x74, 0x5f, 0x61, 0x00, 0x01, // [0x2d] "test_a" kind=0 index=1
    0x06, 0x74, 0x65, 0x73, 0x74, 0x5f, 0x63, 0x00, 0x03, // [0x36] "test_c" kind=0 index=3
    0x09, 0x77, 0x61, 0x73, 0x6d, 0x5f, 0x6c, 0x65, 0x61, 0x66, 0x00, 0x04, // [0x3f] "wasm_leaf" kind=0 index=4

    // Code section
    0x0a, 0x3e,                                      // [0x4b] section id=10, size=62
    0x04,                                            // [0x4d] 4 function bodies
    0x0b,                                            // [0x4e] body size=11  func[1] <test_a>
    0x00,                                            // [0x4f] 0 local declaration groups
    0x41, 0x91, 0xa2, 0xc4, 0x88, 0x01,              // [0x50] i32.const 286331153
    0x10, 0x02,                                      // [0x56] call 2
    0x1a,                                            // [0x58] drop
    0x0b,                                            // [0x59] end
    0x12,                                            // [0x5a] body size=18  func[2]
    0x00,                                            // [0x5b] 0 local declaration groups
    0x41, 0xa2, 0xc4, 0x88, 0x91, 0x02,              // [0x5c] i32.const 572662306
    0x41, 0xb3, 0xe6, 0xcc, 0x99, 0x03,              // [0x62] i32.const 858993459
    0x10, 0x00,                                      // [0x68] call 0 <js.mid_callback>
    0x1a,                                            // [0x6a] drop
    0x1a,                                            // [0x6b] drop
    0x0b,                                            // [0x6c] end
    0x0b,                                            // [0x6d] body size=11  func[3] <test_c>
    0x00,                                            // [0x6e] 0 local declaration groups
    0x41, 0xc4, 0x88, 0x91, 0xa2, 0x04,              // [0x6f] i32.const 1145324612
    0x10, 0x04,                                      // [0x75] call 4 <wasm_leaf>
    0x1a,                                            // [0x77] drop
    0x0b,                                            // [0x78] end
    0x11,                                            // [0x79] body size=17  func[4] <wasm_leaf>
    0x00,                                            // [0x7a] 0 local declaration groups
    0x41, 0xd5, 0xaa, 0xd5, 0xaa, 0x05,              // [0x7b] i32.const 1431655765
    0x41, 0xe6, 0xcc, 0x99, 0xb3, 0x06,              // [0x81] i32.const 1717986918
    0x01,                                            // [0x87] nop  <-- breakpoint
    0x1a,                                            // [0x88] drop
    0x1a,                                            // [0x89] drop
    0x0b,                                            // [0x8a] end
]);

var instance;
var imports = { js: { mid_callback: function () { instance.exports.test_c(); } } };
var module = new WebAssembly.Module(wasm);
instance = new WebAssembly.Instance(module, imports);

let driver = instance.exports.test_a;

print("DEBUGGER_READY");
let iteration = 0;
for (;;) {
    driver();
    iteration += 1;
    if (iteration % 1e6 == 0)
        print("iteration=", iteration);
}
