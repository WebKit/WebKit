// This includes: Nop, Drop, Select, and End
var wasm = new Uint8Array([
    0x00, 0x61, 0x73, 0x6d, // magic
    0x01, 0x00, 0x00, 0x00, // version

    // Type section: (func [] -> [])
    0x01, 0x04, 0x01, 0x60, 0x00, 0x00,

    // Function section: 1 function
    0x03, 0x02, 0x01, 0x00,

    // Export section: export function 0 as "test"
    0x07, 0x08, 0x01, 0x04, 0x74, 0x65, 0x73, 0x74, 0x00, 0x00,

    // Code section
    0x0a,       // section id = 10
    0x16,       // section size = 22 bytes
    0x01,       // 1 function
    0x14,       // function body size = 20 bytes
    0x00,       // 0 local declarations
    // Function body (19 bytes):
    0x01,             // nop
    0x41, 0x2a,       // i32.const 42
    0x1a,             // drop
    0x41, 0x01,       // i32.const 1
    0x41, 0x02,       // i32.const 2
    0x41, 0x01,       // i32.const 1
    0x1b,             // select
    0x1a,             // drop
    0x02, 0x40,       // block
    0x41, 0x05,       // i32.const 5
    0x1a,             // drop
    0x0b,             // end block
    0x0b              // end function
]);

var module = new WebAssembly.Module(wasm);
var instance_step = new WebAssembly.Instance(module);
let test = instance_step.exports.test;

let iteration = 0;
for (; ;) {
    test();
    iteration += 1;
    if (iteration % 1e6 == 0)
        print("iteration=", iteration);
}



