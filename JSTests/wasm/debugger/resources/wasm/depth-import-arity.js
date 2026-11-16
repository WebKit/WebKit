// qWasmStackValue depth for a call into JS taking several parameters, where the same JS function is
// also bound to an import of a different arity. The arity recorded on the JS frame is what picks the
// right import, so this checks it for more than one parameter.
//
//   [0x5d] inner <- test called the three-parameter binding of js.f, keeping 0x11111111, so its depth
//                   is 4: the kept entry plus the three arguments the call consumed.
var wasm = new Uint8Array([
    0x00, 0x61, 0x73, 0x6d,                          // [0x00] magic
    0x01, 0x00, 0x00, 0x00,                          // [0x04] version

    // Type section
    0x01, 0x0e,                                      // [0x08] section id=1, size=14
    0x03,                                            // [0x0a] 3 types
    0x60, 0x03, 0x7f, 0x7f, 0x7f, 0x00,              // [0x0b] type 0: 3 params -> 0 results
    0x60, 0x01, 0x7f, 0x00,                          // [0x11] type 1: 1 params -> 0 results
    0x60, 0x00, 0x00,                                // [0x15] type 2: 0 params -> 0 results

    // Import section
    0x02, 0x0f,                                      // [0x18] section id=2, size=15
    0x02,                                            // [0x1a] 2 entries
    0x02, 0x6a, 0x73, 0x01, 0x66, 0x00, 0x00,        // [0x1b] "js.f" kind=0 index=0
    0x02, 0x6a, 0x73, 0x01, 0x66, 0x00, 0x01,        // [0x22] "js.f" kind=0 index=1

    // Function section
    0x03, 0x03,                                      // [0x29] section id=3, size=3
    0x02,                                            // [0x2b] 2 functions
    0x02, 0x02,                                      // [0x2c] type index per function

    // Export section
    0x07, 0x10,                                      // [0x2e] section id=7, size=16
    0x02,                                            // [0x30] 2 entries
    0x04, 0x74, 0x65, 0x73, 0x74, 0x00, 0x02,        // [0x31] "test" kind=0 index=2
    0x05, 0x69, 0x6e, 0x6e, 0x65, 0x72, 0x00, 0x03,  // [0x38] "inner" kind=0 index=3

    // Code section
    0x0a, 0x1e,                                      // [0x40] section id=10, size=30
    0x02,                                            // [0x42] 2 function bodies
    0x11,                                            // [0x43] body size=17  func[2] <test>
    0x00,                                            // [0x44] 0 local declaration groups
    0x41, 0x91, 0xa2, 0xc4, 0x88, 0x01,              // [0x45] i32.const 286331153
    0x41, 0x01,                                      // [0x4b] i32.const 1
    0x41, 0x02,                                      // [0x4d] i32.const 2
    0x41, 0x03,                                      // [0x4f] i32.const 3
    0x10, 0x00,                                      // [0x51] call 0 <js.f>
    0x1a,                                            // [0x53] drop
    0x0b,                                            // [0x54] end
    0x0a,                                            // [0x55] body size=10  func[3] <inner>
    0x00,                                            // [0x56] 0 local declaration groups
    0x41, 0xd5, 0xaa, 0xd5, 0xaa, 0x05,              // [0x57] i32.const 1431655765
    0x01,                                            // [0x5d] nop  <-- breakpoint
    0x1a,                                            // [0x5e] drop
    0x0b,                                            // [0x5f] end
]);

var module = new WebAssembly.Module(wasm);
var instance;
var imports = { js: { f: function () { instance.exports.inner(); } } };
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
