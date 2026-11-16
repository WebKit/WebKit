// Operand stack depth across WASM -> JS -> WASM -> JS -> WASM, for qWasmStackValue.
// Two wasm->JS crossings, with a different entry count in each wasm frame.
//
// Call chain: JS main -> test_outer[W] -> js.mid_outer -> test_mid[W] -> js.mid_inner -> wasm_inner[W]
//
//   frame #0  wasm_inner  depth 1   index 0 = 0x55555555
//   frame #2  test_mid    depth 2   index 0 = 0x22222222, index 1 = 0x33333333
//   frame #4  test_outer  depth 1   index 0 = 0x11111111
//
var wasm = new Uint8Array([
    0x00, 0x61, 0x73, 0x6d,                          // [0x00] magic
    0x01, 0x00, 0x00, 0x00,                          // [0x04] version

    // Type section
    0x01, 0x04,                                      // [0x08] section id=1, size=4
    0x01,                                            // [0x0a] 1 types
    0x60, 0x00, 0x00,                                // [0x0b] type 0: 0 params -> 0 results

    // Import section
    0x02, 0x1f,                                      // [0x0e] section id=2, size=31
    0x02,                                            // [0x10] 2 entries
    0x02, 0x6a, 0x73, 0x09, 0x6d, 0x69, 0x64, 0x5f, 0x6f, 0x75, 0x74, 0x65, 0x72, 0x00, 0x00, // [0x11] "js.mid_outer" kind=0 index=0
    0x02, 0x6a, 0x73, 0x09, 0x6d, 0x69, 0x64, 0x5f, 0x69, 0x6e, 0x6e, 0x65, 0x72, 0x00, 0x00, // [0x20] "js.mid_inner" kind=0 index=0

    // Function section
    0x03, 0x04,                                      // [0x2f] section id=3, size=4
    0x03,                                            // [0x31] 3 functions
    0x00, 0x00, 0x00,                                // [0x32] type index per function

    // Export section
    0x07, 0x26,                                      // [0x35] section id=7, size=38
    0x03,                                            // [0x37] 3 entries
    0x0a, 0x74, 0x65, 0x73, 0x74, 0x5f, 0x6f, 0x75, 0x74, 0x65, 0x72, 0x00, 0x02, // [0x38] "test_outer" kind=0 index=2
    0x08, 0x74, 0x65, 0x73, 0x74, 0x5f, 0x6d, 0x69, 0x64, 0x00, 0x03, // [0x45] "test_mid" kind=0 index=3
    0x0a, 0x77, 0x61, 0x73, 0x6d, 0x5f, 0x69, 0x6e, 0x6e, 0x65, 0x72, 0x00, 0x04, // [0x50] "wasm_inner" kind=0 index=4

    // Code section
    0x0a, 0x2b,                                      // [0x5d] section id=10, size=43
    0x03,                                            // [0x5f] 3 function bodies
    0x0b,                                            // [0x60] body size=11  func[2] <test_outer>
    0x00,                                            // [0x61] 0 local declaration groups
    0x41, 0x91, 0xa2, 0xc4, 0x88, 0x01,              // [0x62] i32.const 286331153
    0x10, 0x00,                                      // [0x68] call 0 <js.mid_outer>
    0x1a,                                            // [0x6a] drop
    0x0b,                                            // [0x6b] end
    0x12,                                            // [0x6c] body size=18  func[3] <test_mid>
    0x00,                                            // [0x6d] 0 local declaration groups
    0x41, 0xa2, 0xc4, 0x88, 0x91, 0x02,              // [0x6e] i32.const 572662306
    0x41, 0xb3, 0xe6, 0xcc, 0x99, 0x03,              // [0x74] i32.const 858993459
    0x10, 0x01,                                      // [0x7a] call 1 <js.mid_inner>
    0x1a,                                            // [0x7c] drop
    0x1a,                                            // [0x7d] drop
    0x0b,                                            // [0x7e] end
    0x0a,                                            // [0x7f] body size=10  func[4] <wasm_inner>
    0x00,                                            // [0x80] 0 local declaration groups
    0x41, 0xd5, 0xaa, 0xd5, 0xaa, 0x05,              // [0x81] i32.const 1431655765
    0x01,                                            // [0x87] nop  <-- breakpoint
    0x1a,                                            // [0x88] drop
    0x0b,                                            // [0x89] end
]);

var instance;
var imports = {
    js: {
        mid_outer: function () { instance.exports.test_mid(); },
        mid_inner: function () { instance.exports.wasm_inner(); }
    }
};
var module = new WebAssembly.Module(wasm);
instance = new WebAssembly.Instance(module, imports);

let driver = instance.exports.test_outer;

print("DEBUGGER_READY");
let iteration = 0;
for (;;) {
    driver();
    iteration += 1;
    if (iteration % 1e6 == 0)
        print("iteration=", iteration);
}
