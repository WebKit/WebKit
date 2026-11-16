var wasm = new Uint8Array([
    // [0x00] WASM header
    0x00, 0x61, 0x73, 0x6d, // magic: \0asm
    0x01, 0x00, 0x00, 0x00, // version: 1

    // [0x08] Type section: 1 type, func () -> i32
    0x01, 0x05, 0x01, 0x60, 0x00, 0x01, 0x7f,

    // [0x0f] Function section: 1 function, type index 0
    0x03, 0x02, 0x01, 0x00,

    // [0x13] Export section: export function 0 as "triggerTrap"
    0x07, 0x0f, 0x01, 0x0b, 0x74, 0x72, 0x69, 0x67, 0x67, 0x65, 0x72, 0x54, 0x72, 0x61, 0x70, 0x00, 0x00,

    // [0x24] Code section
    0x0a,       // section id = 10
    0x09,       // section size = 9
    0x01,       // 1 function body
    0x07,       // body size = 7
    0x00,       // 0 local declarations
    0x41, 0x0a, // [0x29][0x2a] i32.const 10
    0x41, 0x00, // [0x2b][0x2c] i32.const 0
    0x6d,       // [0x2d] i32.div_s  <- DivisionByZero trap
    0x0b        // [0x2e] end
]);

var module = new WebAssembly.Module(wasm);
var instance = new WebAssembly.Instance(module);
let triggerTrap = instance.exports.triggerTrap;

print("DEBUGGER_READY");
let iteration = 0;
for (;;) {
    try {
        triggerTrap();
    } catch (e) {
    }
    iteration += 1;
    if (iteration % 1e5 == 0)
        print("iteration=", iteration);
}
