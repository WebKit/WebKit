// Two live instances of one named module. Each instance is a library of its own, so LLDB sees
// two images with distinct base addresses, and a stop is reported against whichever instance ran
// into the shared bytecode.
var wasm = new Uint8Array([
    // [0x00] WASM header
    0x00, 0x61, 0x73, 0x6d, // magic: \0asm
    0x01, 0x00, 0x00, 0x00, // version: 1

    // [0x08] Type section: 1 type
    0x01, 0x04,             // section id=1, size=4
    0x01,                   // 1 type
    0x60, 0x00, 0x00,       // Type 0: (func [] -> [])

    // [0x0e] Function section: 1 function
    0x03, 0x02,             // section id=3, size=2
    0x01,                   // 1 function
    0x00,                   // function 0: type 0

    // [0x12] Export section: export function 0 as "func_a"
    0x07, 0x0a,             // section id=7, size=10
    0x01,                   // 1 export
    0x06, 0x66, 0x75, 0x6e, 0x63, 0x5f, 0x61, // name "func_a" (length=6)
    0x00,                   // export kind: function
    0x00,                   // function index 0

    // [0x1e] Code section: 1 function body
    0x0a, 0x08,             // section id=10, size=8
    0x01,                   // 1 function body

    // [0x21] func_a body
    0x06,                   // body size=6
    0x00,                   // 0 local declarations
    0x01,                   // [0x23] nop
    0x41, 0x2a,             // [0x24] i32.const 42
    0x1a,                   // [0x26] drop
    0x0b,                   // [0x27] end

    // [0x28] Name section: custom section "name", subsection 0 = module name "mymodule"
    0x00, 0x10,             // section id=0 (custom), payload size=16
    0x04, 0x6e, 0x61, 0x6d, 0x65,          // custom section name: "name" (length=4)
    0x00, 0x09,             // subsection 0 (module name), subsection size=9
    0x08,                   // module name length=8
    0x6d, 0x79, 0x6d, 0x6f, 0x64, 0x75, 0x6c, 0x65, // "mymodule"
]);

var module = new WebAssembly.Module(wasm);
var running = new WebAssembly.Instance(module);
var idle = new WebAssembly.Instance(module); // Never called, but kept alive for the whole run.

print("DEBUGGER_READY");
let iteration = 0;
for (; ;) {
    running.exports.func_a();
    iteration += 1;
    if (iteration % 1e6 == 0)
        print("iteration=", iteration);
}
