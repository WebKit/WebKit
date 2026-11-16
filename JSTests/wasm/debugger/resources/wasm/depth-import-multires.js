// Operand stack depth across WASM -> JS -> WASM where the import returns MORE values than it takes,
// so the call reserves extraSpaceForReturns; reading back first_non_arg must not shift by it.
//
// Call chain: JS main -> outer[W] -> js.imp -> inner[W]
//
//   frame #0  inner  depth 1   index 0 = 0x55555555
//   frame #2  outer  depth 2   index 0 = 0x11111111 (kept), index 1 = 0x22222222 (the argument)
//
var wasm = new Uint8Array([
    0x00, 0x61, 0x73, 0x6d,                                                  // [0x00] magic
    0x01, 0x00, 0x00, 0x00,                                                  // [0x04] version

    // Type section
    0x01, 0x0a,                                                              // [0x08] section id=1, size=10
    0x02, 0x60, 0x01, 0x7f, 0x02, 0x7f, 0x7f, 0x60, 0x00, 0x00,              // [0x0a]

    // Import section
    0x02, 0x0a,                                                              // [0x14] section id=2, size=10
    0x01, 0x02, 0x6a, 0x73, 0x03, 0x69, 0x6d, 0x70, 0x00, 0x00,              // [0x16]

    // Function section
    0x03, 0x03,                                                              // [0x20] section id=3, size=3
    0x02, 0x01, 0x01,                                                        // [0x22]

    // Export section
    0x07, 0x11,                                                              // [0x25] section id=7, size=17
    0x02, 0x05, 0x6f, 0x75, 0x74, 0x65, 0x72, 0x00, 0x01, 0x05, 0x69, 0x6e,  // [0x27]
    0x6e, 0x65, 0x72, 0x00, 0x02,                                            // [0x33]

    // Code section
    0x0a, 0x20,                                                              // [0x38] section id=10, size=32
    0x02,                                                                    // [0x3a] 2 function bodies
    0x13,                                                                    // [0x3b] body size=19  func[1] <outer>
    0x00,                                                                    // [0x3c] 0 local declaration groups
    0x41, 0x91, 0xa2, 0xc4, 0x88, 0x01,                                      // [0x3d] i32.const 286331153
    0x41, 0xa2, 0xc4, 0x88, 0x91, 0x02,                                      // [0x43] i32.const 572662306
    0x10, 0x00,                                                              // [0x49] call 0 <js.imp>
    0x1a,                                                                    // [0x4b] drop
    0x1a,                                                                    // [0x4c] drop
    0x1a,                                                                    // [0x4d] drop
    0x0b,                                                                    // [0x4e] end
    0x0a,                                                                    // [0x4f] body size=10  func[2] <inner>
    0x00,                                                                    // [0x50] 0 local declaration groups
    0x41, 0xd5, 0xaa, 0xd5, 0xaa, 0x05,                                      // [0x51] i32.const 1431655765
    0x01,                                                                    // [0x57] nop  <-- breakpoint
    0x1a,                                                                    // [0x58] drop
    0x0b,                                                                    // [0x59] end
]);

var instance;
var imports = {
    js: {
        // Two results, so the crossing reserves return space. Re-enters WASM so that outer stays
        // suspended in the call while the breakpoint in inner is live.
        imp: function (x) {
            instance.exports.inner();
            return [x + 1, x + 2];
        }
    }
};

instance = new WebAssembly.Instance(new WebAssembly.Module(wasm), imports);

print("DEBUGGER_READY");
let iteration = 0;
for (;;) {
    instance.exports.outer();
    iteration += 1;
    if (iteration % 1e6 == 0)
        print("iteration=", iteration);
}
