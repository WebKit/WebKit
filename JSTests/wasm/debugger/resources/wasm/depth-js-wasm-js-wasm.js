// Operand stack depth across a JS -> WASM -> JS -> WASM call chain, for qWasmStackValue.
// The wasm frame below the JS callback keeps two entries across the crossing, so its depth must be
// recoverable even though the frame directly beneath it is not a wasm frame.
//
// Call chain: JS main -> test[W] -> js.callback[JS] -> inner[W]
//
//   frame #0  inner   depth 2   index 0 = 0x55555555, index 1 = 0x1122334455667788 (i64)
//   frame #1  JS callback
//   frame #2  test    depth 2   index 0 = 0x11111111, index 1 = 0x22222222
//
var wasm = new Uint8Array([
    0x00, 0x61, 0x73, 0x6d,                          // [0x00] magic
    0x01, 0x00, 0x00, 0x00,                          // [0x04] version

    // Type section
    0x01, 0x04,                                      // [0x08] section id=1, size=4
    0x01,                                            // [0x0a] 1 types
    0x60, 0x00, 0x00,                                // [0x0b] type 0: 0 params -> 0 results

    // Import section
    0x02, 0x0f,                                      // [0x0e] section id=2, size=15
    0x01,                                            // [0x10] 1 entries
    0x02, 0x6a, 0x73, 0x08, 0x63, 0x61, 0x6c, 0x6c, 0x62, 0x61, 0x63, 0x6b, 0x00, 0x00, // [0x11] "js.callback" kind=0 index=0

    // Function section
    0x03, 0x03,                                      // [0x1f] section id=3, size=3
    0x02,                                            // [0x21] 2 functions
    0x00, 0x00,                                      // [0x22] type index per function

    // Export section
    0x07, 0x10,                                      // [0x24] section id=7, size=16
    0x02,                                            // [0x26] 2 entries
    0x04, 0x74, 0x65, 0x73, 0x74, 0x00, 0x01,        // [0x27] "test" kind=0 index=1
    0x05, 0x69, 0x6e, 0x6e, 0x65, 0x72, 0x00, 0x02,  // [0x2e] "inner" kind=0 index=2

    // Code section
    0x0a, 0x2a,                                      // [0x36] section id=10, size=42
    0x02,                                            // [0x38] 2 function bodies
    0x12,                                            // [0x39] body size=18  func[1] <test>
    0x00,                                            // [0x3a] 0 local declaration groups
    0x41, 0x91, 0xa2, 0xc4, 0x88, 0x01,              // [0x3b] i32.const 286331153
    0x41, 0xa2, 0xc4, 0x88, 0x91, 0x02,              // [0x41] i32.const 572662306
    0x10, 0x00,                                      // [0x47] call 0 <js.callback>
    0x1a,                                            // [0x49] drop
    0x1a,                                            // [0x4a] drop
    0x0b,                                            // [0x4b] end
    0x15,                                            // [0x4c] body size=21  func[2] <inner>
    0x00,                                            // [0x4d] 0 local declaration groups
    0x41, 0xd5, 0xaa, 0xd5, 0xaa, 0x05,              // [0x4e] i32.const 1431655765
    0x42, 0x88, 0xef, 0x99, 0xab, 0xc5, 0xe8, 0x8c, 0x91, // [0x54] i64.const 1234605616436508552
    0x11,                                            // [0x5d] 
    0x01,                                            // [0x5e] nop  <-- breakpoint
    0x1a,                                            // [0x5f] drop
    0x1a,                                            // [0x60] drop
    0x0b,                                            // [0x61] end
]);

var instance;
var imports = { js: { callback: function () { instance.exports.inner(); } } };
var module = new WebAssembly.Module(wasm);
instance = new WebAssembly.Instance(module, imports);

let driver = instance.exports.test;

print("DEBUGGER_READY");
let iteration = 0;
for (;;) {
    driver();
    iteration += 1;
    if (iteration % 1e6 == 0)
        print("iteration=", iteration);
}
