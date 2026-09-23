// A tail call from WASM straight into a JS import replaces the calling WASM frame, so the WasmToJS
// stub's caller is whatever entered that function -- here the JS driver, not a WASM frame. There is
// then no outer WASM frame to report, and the PC the stub saved does not belong to one either.
//
// Call chain: JS main -> outer[W] -> (return_call) -> WasmToJS stub -> js.imp -> bp[W]
//
//   frame #0  bp        WASM, stops at [0x3f]
//   frame #1  js.imp    JS
//   frame #2  JS main   -- outer's frame is gone; the stub contributes nothing
//
var wasm = new Uint8Array([
    0x00, 0x61, 0x73, 0x6d,                                                  // [0x00] magic
    0x01, 0x00, 0x00, 0x00,                                                  // [0x04] version

    // Type section
    0x01, 0x04,                                                              // [0x08] section id=1, size=4
    0x01, 0x60, 0x00, 0x00,                                                  // [0x0a]

    // Import section
    0x02, 0x0a,                                                              // [0x0e] section id=2, size=10
    0x01, 0x02, 0x6a, 0x73, 0x03, 0x69, 0x6d, 0x70, 0x00, 0x00,              // [0x10]

    // Function section
    0x03, 0x03,                                                              // [0x1a] section id=3, size=3
    0x02, 0x00, 0x00,                                                        // [0x1c]

    // Export section
    0x07, 0x0e,                                                              // [0x1f] section id=7, size=14
    0x02, 0x05, 0x6f, 0x75, 0x74, 0x65, 0x72, 0x00, 0x01, 0x02, 0x62, 0x70,  // [0x21]
    0x00, 0x02,                                                              // [0x2d]

    // Code section
    0x0a, 0x11,                                                              // [0x2f] section id=10, size=17
    0x02,                                                                    // [0x31] 2 function bodies
    0x04,                                                                    // [0x32] body size=4  func[1] <outer>
    0x00,                                                                    // [0x33] 0 local declaration groups
    0x12, 0x00,                                                              // [0x34] return_call 0
    0x0b,                                                                    // [0x36] end
    0x0a,                                                                    // [0x37] body size=10  func[2] <bp>
    0x00,                                                                    // [0x38] 0 local declaration groups
    0x41, 0xd5, 0xaa, 0xd5, 0xaa, 0x05,                                      // [0x39] i32.const 1431655765
    0x01,                                                                    // [0x3f] nop  <-- breakpoint
    0x1a,                                                                    // [0x40] drop
    0x0b,                                                                    // [0x41] end
]);

var instance;
var imports = { js: { imp: function () { instance.exports.bp(); } } };
instance = new WebAssembly.Instance(new WebAssembly.Module(wasm), imports);

print("DEBUGGER_READY");
let iteration = 0;
for (;;) {
    instance.exports.outer();
    iteration += 1;
    if (iteration % 1e6 == 0)
        print("iteration=", iteration);
}
