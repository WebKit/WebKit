// qWasmStackValue depth for a frame suspended in a call it made from inside a catch handler. Catch
// entry recomputes the frame's stack pointer from catch metadata rather than unwinding it, so this
// checks that the operand stack top the later call saved is still found.
//
//   [0x4a] leaf <- catcher, which kept 0x11111111 across the try/catch and pushed 0x22222222 inside
//                  the handler before calling, so its depth is 2.
var wasm = new Uint8Array([
    0x00, 0x61, 0x73, 0x6d,                          // [0x00] magic
    0x01, 0x00, 0x00, 0x00,                          // [0x04] version

    // Type section
    0x01, 0x04,                                      // [0x08] section id=1, size=4
    0x01,                                            // [0x0a] 1 types
    0x60, 0x00, 0x00,                                // [0x0b] type 0: 0 params -> 0 results

    // Function section
    0x03, 0x03,                                      // [0x0e] section id=3, size=3
    0x02,                                            // [0x10] 2 functions
    0x00, 0x00,                                      // [0x11] type index per function

    // id=13 section
    0x0d, 0x03,                                      // [0x13] section id=13, size=3
    0x01, 0x00, 0x00,                                // [0x15] 

    // Export section
    0x07, 0x0b,                                      // [0x18] section id=7, size=11
    0x01,                                            // [0x1a] 1 entries
    0x07, 0x63, 0x61, 0x74, 0x63, 0x68, 0x65, 0x72, 0x00, 0x00, // [0x1b] "catcher" kind=0 index=0

    // Code section
    0x0a, 0x26,                                      // [0x25] section id=10, size=38
    0x02,                                            // [0x27] 2 function bodies
    0x19,                                            // [0x28] body size=25  func[0] <catcher>
    0x00,                                            // [0x29] 0 local declaration groups
    0x41, 0x91, 0xa2, 0xc4, 0x88, 0x01,              // [0x2a] i32.const 286331153
    0x06, 0x40,                                      // [0x30] try
    0x08, 0x00,                                      // [0x32] throw 0
    0x07, 0x00,                                      // [0x34] catch 0
    0x41, 0xa2, 0xc4, 0x88, 0x91, 0x02,              // [0x36] i32.const 572662306
    0x10, 0x01,                                      // [0x3c] call 1
    0x1a,                                            // [0x3e] drop
    0x0b,                                            // [0x3f] end
    0x1a,                                            // [0x40] drop
    0x0b,                                            // [0x41] end
    0x0a,                                            // [0x42] body size=10  func[1]
    0x00,                                            // [0x43] 0 local declaration groups
    0x41, 0xd5, 0xaa, 0xd5, 0xaa, 0x05,              // [0x44] i32.const 1431655765
    0x01,                                            // [0x4a] nop  <-- breakpoint
    0x1a,                                            // [0x4b] drop
    0x0b,                                            // [0x4c] end
]);

var module = new WebAssembly.Module(wasm);
var instance = new WebAssembly.Instance(module);

let driver = instance.exports.catcher;

print("DEBUGGER_READY");
let iteration = 0;
for (;;) {
    driver();
    iteration += 1;
    if (iteration % 1e6 == 0)
        print("iteration=", iteration);
}
