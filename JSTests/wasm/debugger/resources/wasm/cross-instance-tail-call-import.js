// The same restore frame, but above a WasmToJS stub: call_indirect targets the funcref's owning
// instance, and a re-exported JS import is isJS() yet owned elsewhere -- so this crossing is both.
//
// Instance 0 is module A (imports js.f and re-exports it), instance 1 is module B.
//
// Call chain: JS main -> B.caller[W] -> B.tc[W] -> (return_call_indirect) -> A's WasmToJS stub
//             -> js.f -> B.bp[W]
//
//   frame #0  B.bp      depth 1   index 0 = 0x55555555
//   frame #2  B.caller  depth 1   index 0 = 0x11111111   (B.tc's frame was replaced by the tail call)
//
var wasmA = new Uint8Array([
    0x00, 0x61, 0x73, 0x6d,                          // [0x00] magic
    0x01, 0x00, 0x00, 0x00,                          // [0x04] version

    // Type section
    0x01, 0x04,                                      // [0x08] section id=1, size=4
    0x01, 0x60, 0x00, 0x00,                          // [0x0a]

    // Import section
    0x02, 0x08,                                      // [0x0e] section id=2, size=8
    0x01, 0x02, 0x6a, 0x73, 0x01, 0x66, 0x00, 0x00,  // [0x10]

    // Export section
    0x07, 0x05,                                      // [0x18] section id=7, size=5
    0x01, 0x01, 0x66, 0x00, 0x00,                    // [0x1a]
]);

var wasmB = new Uint8Array([
    0x00, 0x61, 0x73, 0x6d,                                                  // [0x00] magic
    0x01, 0x00, 0x00, 0x00,                                                  // [0x04] version

    // Type section
    0x01, 0x04,                                                              // [0x08] section id=1, size=4
    0x01, 0x60, 0x00, 0x00,                                                  // [0x0a]

    // Import section
    0x02, 0x0e,                                                              // [0x0e] section id=2, size=14
    0x01, 0x02, 0x6a, 0x73, 0x05, 0x74, 0x61, 0x62, 0x6c, 0x65, 0x01, 0x70,  // [0x10]
    0x00, 0x01,                                                              // [0x1c]

    // Function section
    0x03, 0x04,                                                              // [0x1e] section id=3, size=4
    0x03, 0x00, 0x00, 0x00,                                                  // [0x20]

    // Export section
    0x07, 0x0f,                                                              // [0x24] section id=7, size=15
    0x02, 0x06, 0x63, 0x61, 0x6c, 0x6c, 0x65, 0x72, 0x00, 0x00, 0x02, 0x62,  // [0x26]
    0x70, 0x00, 0x02,                                                        // [0x32]

    // Code section
    0x0a, 0x20,                                                              // [0x35] section id=10, size=32
    0x03,                                                                    // [0x37] 3 function bodies
    0x0b,                                                                    // [0x38] body size=11  func[0] <caller>
    0x00,                                                                    // [0x39] 0 local declaration groups
    0x41, 0x91, 0xa2, 0xc4, 0x88, 0x01,                                      // [0x3a] i32.const 286331153
    0x10, 0x01,                                                              // [0x40] call 1
    0x1a,                                                                    // [0x42] drop
    0x0b,                                                                    // [0x43] end
    0x07,                                                                    // [0x44] body size=7  func[1]
    0x00,                                                                    // [0x45] 0 local declaration groups
    0x41, 0x00,                                                              // [0x46] i32.const 0
    0x13, 0x00, 0x00,                                                        // [0x48] return_call_indirect 0 0
    0x0b,                                                                    // [0x4b] end
    0x0a,                                                                    // [0x4c] body size=10  func[2] <bp>
    0x00,                                                                    // [0x4d] 0 local declaration groups
    0x41, 0xd5, 0xaa, 0xd5, 0xaa, 0x05,                                      // [0x4e] i32.const 1431655765
    0x01,                                                                    // [0x54] nop  <-- breakpoint
    0x1a,                                                                    // [0x55] drop
    0x0b,                                                                    // [0x56] end
]);

var table = new WebAssembly.Table({ element: "anyfunc", initial: 1 });
var instanceB;

// Module A is instantiated first, so it owns instance 0 and module B owns instance 1.
var instanceA = new WebAssembly.Instance(new WebAssembly.Module(wasmA), {
    js: { f: function () { instanceB.exports.bp(); } }
});

// A re-exported JS import: isJS(), but owned by instance 0 rather than by the caller.
table.set(0, instanceA.exports.f);

instanceB = new WebAssembly.Instance(new WebAssembly.Module(wasmB), { js: { table: table } });

print("DEBUGGER_READY");
let iteration = 0;
for (;;) {
    instanceB.exports.caller();
    iteration += 1;
    if (iteration % 1e6 == 0)
        print("iteration=", iteration);
}
