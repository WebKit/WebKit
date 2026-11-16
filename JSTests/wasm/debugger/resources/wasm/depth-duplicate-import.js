// qWasmStackValue depth when one JS function is bound to several imports. That is legal -- JS functions
// are untyped -- so the imported function alone does not identify which call a wasm frame has in flight.
// The declared arity does, and where arity still leaves several candidates they have to agree on the
// frame size the call reserved, or the depth would be ambiguous.
//
//   [0x8a] leaf_arity  <- test_arity called the 0-parameter binding of js.f, though js.f is also
//                         imported with one parameter. Kept 0x11111111, so depth 1.
//   [0xa3] leaf_types  <- test_types called the (param i32) binding of js.g, also imported as
//                         (param f32). Kept 0x22222222 and passed 7, so depth 2.
var wasm = new Uint8Array([
    0x00, 0x61, 0x73, 0x6d,                          // [0x00] magic
    0x01, 0x00, 0x00, 0x00,                          // [0x04] version

    // Type section
    0x01, 0x0c,                                      // [0x08] section id=1, size=12
    0x03,                                            // [0x0a] 3 types
    0x60, 0x00, 0x00,                                // [0x0b] type 0: 0 params -> 0 results
    0x60, 0x01, 0x7f, 0x00,                          // [0x0e] type 1: 1 params -> 0 results
    0x60, 0x01, 0x7d, 0x00,                          // [0x12] type 2: 1 params -> 0 results

    // Import section
    0x02, 0x1d,                                      // [0x16] section id=2, size=29
    0x04,                                            // [0x18] 4 entries
    0x02, 0x6a, 0x73, 0x01, 0x66, 0x00, 0x00,        // [0x19] "js.f" kind=0 index=0
    0x02, 0x6a, 0x73, 0x01, 0x66, 0x00, 0x01,        // [0x20] "js.f" kind=0 index=1
    0x02, 0x6a, 0x73, 0x01, 0x67, 0x00, 0x01,        // [0x27] "js.g" kind=0 index=1
    0x02, 0x6a, 0x73, 0x01, 0x67, 0x00, 0x02,        // [0x2e] "js.g" kind=0 index=2

    // Function section
    0x03, 0x05,                                      // [0x35] section id=3, size=5
    0x04,                                            // [0x37] 4 functions
    0x00, 0x00, 0x00, 0x00,                          // [0x38] type index per function

    // Export section
    0x07, 0x35,                                      // [0x3c] section id=7, size=53
    0x04,                                            // [0x3e] 4 entries
    0x0a, 0x74, 0x65, 0x73, 0x74, 0x5f, 0x61, 0x72, 0x69, 0x74, 0x79, 0x00, 0x04, // [0x3f] "test_arity" kind=0 index=4
    0x0a, 0x74, 0x65, 0x73, 0x74, 0x5f, 0x74, 0x79, 0x70, 0x65, 0x73, 0x00, 0x06, // [0x4c] "test_types" kind=0 index=6
    0x0a, 0x6c, 0x65, 0x61, 0x66, 0x5f, 0x61, 0x72, 0x69, 0x74, 0x79, 0x00, 0x05, // [0x59] "leaf_arity" kind=0 index=5
    0x0a, 0x6c, 0x65, 0x61, 0x66, 0x5f, 0x74, 0x79, 0x70, 0x65, 0x73, 0x00, 0x07, // [0x66] "leaf_types" kind=0 index=7

    // Code section
    0x0a, 0x31,                                      // [0x73] section id=10, size=49
    0x04,                                            // [0x75] 4 function bodies
    0x0b,                                            // [0x76] body size=11  func[4] <test_arity>
    0x00,                                            // [0x77] 0 local declaration groups
    0x41, 0x91, 0xa2, 0xc4, 0x88, 0x01,              // [0x78] i32.const 286331153
    0x10, 0x00,                                      // [0x7e] call 0 <js.f>
    0x1a,                                            // [0x80] drop
    0x0b,                                            // [0x81] end
    0x0a,                                            // [0x82] body size=10  func[5] <leaf_arity>
    0x00,                                            // [0x83] 0 local declaration groups
    0x41, 0xd5, 0xaa, 0xd5, 0xaa, 0x05,              // [0x84] i32.const 1431655765
    0x01,                                            // [0x8a] nop  <-- breakpoint
    0x1a,                                            // [0x8b] drop
    0x0b,                                            // [0x8c] end
    0x0d,                                            // [0x8d] body size=13  func[6] <test_types>
    0x00,                                            // [0x8e] 0 local declaration groups
    0x41, 0xa2, 0xc4, 0x88, 0x91, 0x02,              // [0x8f] i32.const 572662306
    0x41, 0x07,                                      // [0x95] i32.const 7
    0x10, 0x02,                                      // [0x97] call 2 <js.g>
    0x1a,                                            // [0x99] drop
    0x0b,                                            // [0x9a] end
    0x0a,                                            // [0x9b] body size=10  func[7] <leaf_types>
    0x00,                                            // [0x9c] 0 local declaration groups
    0x41, 0xe6, 0xcc, 0x99, 0xb3, 0x06,              // [0x9d] i32.const 1717986918
    0x01,                                            // [0xa3] nop  <-- breakpoint
    0x1a,                                            // [0xa4] drop
    0x0b,                                            // [0xa5] end
]);

var module = new WebAssembly.Module(wasm);
var instance;
var imports = {
    js: {
        f: function () { instance.exports.leaf_arity(); },
        g: function () { instance.exports.leaf_types(); }
    }
};
instance = new WebAssembly.Instance(module, imports);

let shape = 0;
function driver() {
    if (shape++ % 2)
        instance.exports.test_types();
    else
        instance.exports.test_arity();
}

print("DEBUGGER_READY");
let iteration = 0;
for (;;) {
    driver();
    iteration += 1;
    if (iteration % 1e6 == 0)
        print("iteration=", iteration);
}
