// Operand stack depth across JS -> JS -> WASM -> JS -> JS -> WASM, for qWasmStackValue.
// Several JS frames sit between the two wasm frames, so the wasm frame's depth must be found without
// relying on the frame directly beneath it.
//
// Call chain: JS main -> js_a -> js_b -> test[W] -> js.callback -> js_c -> inner[W]
//
//   frame #0  inner   depth 1   index 0 = 0x55555555
//   frame #3  test    depth 3   index 0 = 0x11111111, index 1 = 0x22222222, index 2 = 0x33333333
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
    0x0a, 0x26,                                      // [0x36] section id=10, size=38
    0x02,                                            // [0x38] 2 function bodies
    0x19,                                            // [0x39] body size=25  func[1] <test>
    0x00,                                            // [0x3a] 0 local declaration groups
    0x41, 0x91, 0xa2, 0xc4, 0x88, 0x01,              // [0x3b] i32.const 286331153
    0x41, 0xa2, 0xc4, 0x88, 0x91, 0x02,              // [0x41] i32.const 572662306
    0x41, 0xb3, 0xe6, 0xcc, 0x99, 0x03,              // [0x47] i32.const 858993459
    0x10, 0x00,                                      // [0x4d] call 0 <js.callback>
    0x1a,                                            // [0x4f] drop
    0x1a,                                            // [0x50] drop
    0x1a,                                            // [0x51] drop
    0x0b,                                            // [0x52] end
    0x0a,                                            // [0x53] body size=10  func[2] <inner>
    0x00,                                            // [0x54] 0 local declaration groups
    0x41, 0xd5, 0xaa, 0xd5, 0xaa, 0x05,              // [0x55] i32.const 1431655765
    0x01,                                            // [0x5b] nop  <-- breakpoint
    0x1a,                                            // [0x5c] drop
    0x0b,                                            // [0x5d] end
]);

var instance;
function js_c() { instance.exports.inner(); }
function callback() { js_c(); }
var imports = { js: { callback: callback } };
var module = new WebAssembly.Module(wasm);
instance = new WebAssembly.Instance(module, imports);

function js_b() { instance.exports.test(); }
function js_a() { js_b(); }
let driver = js_a;

print("DEBUGGER_READY");
let iteration = 0;
for (;;) {
    driver();
    iteration += 1;
    if (iteration % 1e6 == 0)
        print("iteration=", iteration);
}
