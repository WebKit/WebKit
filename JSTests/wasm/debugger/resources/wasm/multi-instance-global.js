// WASM debugger test: qWasmGlobal must read the named instance's own global.
//
// Two instances of one module hold independent copies of global 0, so reading it through
// instance 0 and instance 1 must give different values.
//
// Module layout (single function "setg", which stores its parameter into global 0):
//   [0x2a] local.get 0
//   [0x2c] global.set 0
//   [0x2e] end

var wasm = new Uint8Array([
    // [0x00] WASM header
    0x00, 0x61, 0x73, 0x6d, // magic: \0asm
    0x01, 0x00, 0x00, 0x00, // version: 1

    // [0x08] Type section: 1 type, func (i32) -> ()
    0x01, 0x05, 0x01, 0x60, 0x01, 0x7f, 0x00,

    // [0x0f] Function section: 1 function, type index 0
    0x03, 0x02, 0x01, 0x00,

    // [0x13] Global section: 1 global, mutable i32, init i32.const 0
    0x06, 0x06, 0x01,
    0x7f, 0x01,             // i32, mutable
    0x41, 0x00, 0x0b,       // init expr: i32.const 0, end

    // [0x1b] Export section: export function 0 as "setg"
    0x07, 0x08, 0x01,
    0x04, 0x73, 0x65, 0x74, 0x67, // "setg" (length=4)
    0x00, 0x00,             // kind=func, func 0

    // [0x25] Code section
    0x0a,                   // section id = 10
    0x08,                   // section size = 8
    0x01,                   // 1 function body

    // [0x28] setg body: global 0 = param 0
    0x06,                   // body size = 6
    0x00,                   // 0 local declarations
    0x20, 0x00,             // [0x2a] local.get 0
    0x24, 0x00,             // [0x2c] global.set 0
    0x0b,                   // [0x2e] end
]);

var module = new WebAssembly.Module(wasm);
var first = new WebAssembly.Instance(module);   // instance 0
var second = new WebAssembly.Instance(module);  // instance 1

// Byte-repeating, so the expected hex is endianness-independent.
first.exports.setg(0x11111111);
second.exports.setg(0x22222222);

print("DEBUGGER_READY");
let iteration = 0;
for (;;) {
    iteration += 1;
    if (iteration % 1e8 == 0)
        print("iteration=", iteration);
}
