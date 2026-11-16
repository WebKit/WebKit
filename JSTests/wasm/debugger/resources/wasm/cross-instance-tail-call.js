// A cross-instance tail call splices a synthetic RestoreFrameCallee frame between the tail callee
// and the frame it returns to; collectCallStack has to step over it and report what is above.
//
// Instance 0 is module B, instance 1 is module A:
//
//   A.outer  pushes 0x11111111, calls A.mid                    <- frame #1, depth 1
//   A.mid    return_call's B.target, which belongs to instance 0 -- cross-instance, so its own
//            frame is replaced by the tail callee's and a restore frame is inserted
//   B.target pushes 0x77777777 and stops at [0x29]             <- frame #0, depth 1
var wasmB = new Uint8Array([
    0x00, 0x61, 0x73, 0x6d,                                      // [0x00] magic
    0x01, 0x00, 0x00, 0x00,                                      // [0x04] version

    // Type section
    0x01, 0x04,                                                  // [0x08] section id=1, size=4
    0x01, 0x60, 0x00, 0x00,                                      // [0x0a]

    // Function section
    0x03, 0x02,                                                  // [0x0e] section id=3, size=2
    0x01, 0x00,                                                  // [0x10]

    // Export section
    0x07, 0x0a,                                                  // [0x12] section id=7, size=10
    0x01, 0x06, 0x74, 0x61, 0x72, 0x67, 0x65, 0x74, 0x00, 0x00,  // [0x14]

    // Code section
    0x0a, 0x0c,                                                  // [0x1e] section id=10, size=12
    0x01,                                                        // [0x20] 1 function bodies
    0x0a,                                                        // [0x21] body size=10  func[0] <target>
    0x00,                                                        // [0x22] 0 local declaration groups
    0x41, 0xf7, 0xee, 0xdd, 0xbb, 0x07,                          // [0x23] i32.const 2004318071
    0x01,                                                        // [0x29] nop  <-- breakpoint
    0x1a,                                                        // [0x2a] drop
    0x0b,                                                        // [0x2b] end
]);

var wasmA = new Uint8Array([
    0x00, 0x61, 0x73, 0x6d,                                                  // [0x00] magic
    0x01, 0x00, 0x00, 0x00,                                                  // [0x04] version

    // Type section
    0x01, 0x04,                                                              // [0x08] section id=1, size=4
    0x01, 0x60, 0x00, 0x00,                                                  // [0x0a]

    // Import section
    0x02, 0x0c,                                                              // [0x0e] section id=2, size=12
    0x01, 0x01, 0x62, 0x06, 0x74, 0x61, 0x72, 0x67, 0x65, 0x74, 0x00, 0x00,  // [0x10]

    // Function section
    0x03, 0x03,                                                              // [0x1c] section id=3, size=3
    0x02, 0x00, 0x00,                                                        // [0x1e]

    // Export section
    0x07, 0x09,                                                              // [0x21] section id=7, size=9
    0x01, 0x05, 0x6f, 0x75, 0x74, 0x65, 0x72, 0x00, 0x01,                    // [0x23]

    // Code section
    0x0a, 0x12,                                                              // [0x2c] section id=10, size=18
    0x02,                                                                    // [0x2e] 2 function bodies
    0x0b,                                                                    // [0x2f] body size=11  func[1] <outer>
    0x00,                                                                    // [0x30] 0 local declaration groups
    0x41, 0x91, 0xa2, 0xc4, 0x88, 0x01,                                      // [0x31] i32.const 286331153
    0x10, 0x02,                                                              // [0x37] call 2
    0x1a,                                                                    // [0x39] drop
    0x0b,                                                                    // [0x3a] end
    0x04,                                                                    // [0x3b] body size=4  func[2]
    0x00,                                                                    // [0x3c] 0 local declaration groups
    0x12, 0x00,                                                              // [0x3d] return_call 0
    0x0b,                                                                    // [0x3f] end
]);

// Module B is instantiated first, so it owns instance 0 and module A owns instance 1.
var instanceB = new WebAssembly.Instance(new WebAssembly.Module(wasmB));
var instanceA = new WebAssembly.Instance(new WebAssembly.Module(wasmA), { b: { target: instanceB.exports.target } });

print("DEBUGGER_READY");
let iteration = 0;
for (;;) {
    instanceA.exports.outer();
    iteration += 1;
    if (iteration % 1e6 == 0)
        print("iteration=", iteration);
}
