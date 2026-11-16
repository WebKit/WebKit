// Operand stack depth for call_ref and return_call_ref, for qWasmStackValue. Both are base-space
// opcodes (0x14 / 0x15, function-references), not GC. Each leaf has exactly one caller.
//
//   [0x4d] leaf_ref     <- c_ref     kept 0x11111111, then pushed the funcref call_ref consumed
//   [0x6b] leaf_retref  <- mid_ref tail-called it through return_call_ref, so frame #1 is c_retref,
//                          which kept 0x33333333
var wasm = new Uint8Array([
    0x00, 0x61, 0x73, 0x6d,                          // [0x00] magic
    0x01, 0x00, 0x00, 0x00,                          // [0x04] version

    // Type section
    0x01, 0x04,                                      // [0x08] section id=1, size=4
    0x01,                                            // [0x0a] 1 types
    0x60, 0x00, 0x00,                                // [0x0b] type 0: 0 params -> 0 results

    // Function section
    0x03, 0x06,                                      // [0x0e] section id=3, size=6
    0x05,                                            // [0x10] 5 functions
    0x00, 0x00, 0x00, 0x00, 0x00,                    // [0x11] type index per function

    // Export section
    0x07, 0x14,                                      // [0x16] section id=7, size=20
    0x02,                                            // [0x18] 2 entries
    0x05, 0x63, 0x5f, 0x72, 0x65, 0x66, 0x00, 0x00,  // [0x19] "c_ref" kind=0 index=0
    0x08, 0x63, 0x5f, 0x72, 0x65, 0x74, 0x72, 0x65, 0x66, 0x00, 0x02, // [0x21] "c_retref" kind=0 index=2

    // Element section
    0x09, 0x06,                                      // [0x2c] section id=9, size=6
    0x01, 0x03, 0x00, 0x02, 0x01, 0x04,              // [0x2e] 

    // Code section
    0x0a, 0x38,                                      // [0x34] section id=10, size=56
    0x05,                                            // [0x36] 5 function bodies
    0x0d,                                            // [0x37] body size=13  func[0] <c_ref>
    0x00,                                            // [0x38] 0 local declaration groups
    0x41, 0x91, 0xa2, 0xc4, 0x88, 0x01,              // [0x39] i32.const 286331153
    0xd2, 0x01,                                      // [0x3f] ref.func 1
    0x14, 0x00,                                      // [0x41] call_ref (ref null 0)
    0x1a,                                            // [0x43] drop
    0x0b,                                            // [0x44] end
    0x0a,                                            // [0x45] body size=10  func[1]
    0x00,                                            // [0x46] 0 local declaration groups
    0x41, 0xa2, 0xc4, 0x88, 0x91, 0x02,              // [0x47] i32.const 572662306
    0x01,                                            // [0x4d] nop  <-- breakpoint
    0x1a,                                            // [0x4e] drop
    0x0b,                                            // [0x4f] end
    0x0b,                                            // [0x50] body size=11  func[2] <c_retref>
    0x00,                                            // [0x51] 0 local declaration groups
    0x41, 0xb3, 0xe6, 0xcc, 0x99, 0x03,              // [0x52] i32.const 858993459
    0x10, 0x03,                                      // [0x58] call 3
    0x1a,                                            // [0x5a] drop
    0x0b,                                            // [0x5b] end
    0x06,                                            // [0x5c] body size=6  func[3]
    0x00,                                            // [0x5d] 0 local declaration groups
    0xd2, 0x04,                                      // [0x5e] ref.func 4
    0x15, 0x00,                                      // [0x60] return_call_ref <invalid>
    0x0b,                                            // [0x62] end
    0x0a,                                            // [0x63] body size=10  func[4]
    0x00,                                            // [0x64] 0 local declaration groups
    0x41, 0xc4, 0x88, 0x91, 0xa2, 0x04,              // [0x65] i32.const 1145324612
    0x01,                                            // [0x6b] nop  <-- breakpoint
    0x1a,                                            // [0x6c] drop
    0x0b,                                            // [0x6d] end
]);

var module = new WebAssembly.Module(wasm);
var instance = new WebAssembly.Instance(module);

let shape = 0;
function driver() {
    if (shape++ % 2)
        instance.exports.c_retref();
    else
        instance.exports.c_ref();
}

print("DEBUGGER_READY");
let iteration = 0;
for (;;) {
    driver();
    iteration += 1;
    if (iteration % 1e6 == 0)
        print("iteration=", iteration);
}
