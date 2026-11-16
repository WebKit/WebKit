// Three live instances of one named module: two running, one idle. Each instance is a separate
// library with its own base address, and all three share the underlying bytecode.
//
// func_outer calls func_a so a stop has a caller frame. A stop reported for the wrong instance
// shows frame #0 and frame #1 in different libraries.
var wasm = new Uint8Array([
    // [0x00] WASM header
    0x00, 0x61, 0x73, 0x6d, // magic: \0asm
    0x01, 0x00, 0x00, 0x00, // version: 1

    // [0x08] Type section: 1 type
    0x01, 0x04,             // section id=1, size=4
    0x01,                   // 1 type
    0x60, 0x00, 0x00,       // Type 0: (func [] -> [])

    // [0x0e] Function section: 2 functions
    0x03, 0x03,             // section id=3, size=3
    0x02,                   // 2 functions
    0x00,                   // function 0 (func_a): type 0
    0x00,                   // function 1 (func_outer): type 0

    // [0x13] Export section: export function 1 as "func_outer"
    0x07, 0x0e,             // section id=7, size=14
    0x01,                   // 1 export
    0x0a, 0x66, 0x75, 0x6e, 0x63, 0x5f, 0x6f, 0x75, 0x74, 0x65, 0x72, // name "func_outer" (length=10)
    0x00,                   // export kind: function
    0x01,                   // function index 1

    // [0x23] Code section: 2 function bodies
    0x0a, 0x0a,             // section id=10, size=10
    0x02,                   // 2 function bodies

    // [0x26] func_a body
    0x03,                   // body size=3
    0x00,                   // 0 local declarations
    0x01,                   // [0x28] nop
    0x0b,                   // [0x29] end

    // [0x2a] func_outer body
    0x04,                   // body size=4
    0x00,                   // 0 local declarations
    0x10, 0x00,             // [0x2c] call 0 (func_a)
    0x0b,                   // [0x2e] end -- func_a's return PC

    // [0x2f] Name section: custom section "name", subsection 0 = module name "mymodule"
    0x00, 0x10,             // section id=0 (custom), payload size=16
    0x04, 0x6e, 0x61, 0x6d, 0x65,          // custom section name: "name" (length=4)
    0x00, 0x09,             // subsection 0 (module name), subsection size=9
    0x08,                   // module name length=8
    0x6d, 0x79, 0x6d, 0x6f, 0x64, 0x75, 0x6c, 0x65, // "mymodule"
]);

var module = new WebAssembly.Module(wasm);
var first = new WebAssembly.Instance(module);
var second = new WebAssembly.Instance(module);
var idle = new WebAssembly.Instance(module); // Never called, but kept alive for the whole run.

print("DEBUGGER_READY");
let iteration = 0;
for (; ;) {
    first.exports.func_outer();
    second.exports.func_outer();
    iteration += 1;
    if (iteration % 1e6 == 0)
        print("iteration=", iteration);
}
