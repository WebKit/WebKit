// This includes: Nop, Drop, Select, and End
var wasm = new Uint8Array([
    // [0x00] WASM header
    0x00, 0x61, 0x73, 0x6d, // magic
    0x01, 0x00, 0x00, 0x00, // version

    // [0x08] Type section: (func [] -> [])
    0x01, 0x04, 0x01, 0x60, 0x00, 0x00,

    // [0x0e] Function section: 1 function
    0x03, 0x02, 0x01, 0x00,

    // [0x12] Export section: export function 0 as "test"
    0x07, 0x08, 0x01, 0x04, 0x74, 0x65, 0x73, 0x74, 0x00, 0x00,

    // [0x1c] Code section
    0x0a,                   // section id=10
    0x16,                   // section size=22
    0x01,                   // 1 function
    0x14,                   // function body size=20
    0x00,                   // 0 local declarations
    0x01,                   // [0x21] nop
    0x41, 0x2a,             // [0x22] i32.const 42
    0x1a,                   // [0x24] drop
    0x41, 0x01,             // [0x25] i32.const 1
    0x41, 0x02,             // [0x27] i32.const 2
    0x41, 0x01,             // [0x29] i32.const 1
    0x1b,                   // [0x2b] select
    0x1a,                   // [0x2c] drop
    0x02, 0x40,             // [0x2d] block
    0x41, 0x05,             // [0x2f] i32.const 5
    0x1a,                   // [0x31] drop
    0x0b,                   // [0x32] end block
    0x0b                    // [0x33] end function
]);

var module = new WebAssembly.Module(wasm);
var instance_step = new WebAssembly.Instance(module);
let test = instance_step.exports.test;

print("DEBUGGER_READY");
let iteration = 0;
for (; ;) {
    test();
    iteration += 1;
    if (iteration % 1e6 == 0)
        print("iteration=", iteration);
}



