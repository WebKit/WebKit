// Test case for a pure WASM call chain: JS -> WASM_A -> WASM_B -> WASM_C.
// Exercises multiple consecutive WASM->WASM (IPInt CFR-8) hops with no JS in between.
//
// Function index mapping:
//   0: test        (WASM, exported) — calls wasm_middle
//   1: wasm_middle (WASM)           — calls wasm_inner
//   2: wasm_inner  (WASM, exported) — nop, breakpoint target
//
// Expected bt at 0x400000000000003a:
//   frame #0: 0x400000000000003a   (wasm_inner nop — breakpoint)
//   frame #1: 0x4000000000000037   (wasm_middle return PC, after call wasm_inner)
//   frame #2: 0x4000000000000032   (test return PC, after call wasm_middle)
//   frame #3: 0xc000000000000000   (JS main loop)
var wasm = new Uint8Array([
    // [0x00] WASM header
    0x00, 0x61, 0x73, 0x6d, // magic
    0x01, 0x00, 0x00, 0x00, // version

    // [0x08] Type section: 1 type
    0x01, 0x04,             // section id=1, size=4
    0x01,                   // 1 type
    0x60, 0x00, 0x00,       // type 0: (func [] -> [])

    // [0x0e] Function section: 3 functions (all type 0)
    0x03, 0x04,             // section id=3, size=4
    0x03,                   // 3 functions
    0x00, 0x00, 0x00,       // all type 0

    // [0x14] Export section: test (func 0), wasm_inner (func 2)
    0x07, 0x15,             // section id=7, size=21
    0x02,                   // 2 exports
    0x04, 0x74, 0x65, 0x73, 0x74,                                   // name "test" (length=4)
    0x00, 0x00,             // kind: function, index 0
    0x0a, 0x77, 0x61, 0x73, 0x6d, 0x5f, 0x69, 0x6e, 0x6e, 0x65, 0x72, // name "wasm_inner" (length=10)
    0x00, 0x02,             // kind: function, index 2

    // [0x2b] Code section
    0x0a, 0x0f,             // section id=10, size=15
    0x03,                   // 3 functions

    // [0x2e] Function 0 (test): calls wasm_middle
    0x04,                   // body size=4
    0x00,                   // 0 local declarations
    0x10, 0x01,             // [0x30] call 1 (wasm_middle)
    0x0b,                   // [0x32] end

    // [0x33] Function 1 (wasm_middle): calls wasm_inner
    0x04,                   // body size=4
    0x00,                   // 0 local declarations
    0x10, 0x02,             // [0x35] call 2 (wasm_inner)
    0x0b,                   // [0x37] end

    // [0x38] Function 2 (wasm_inner): breakpoint target
    0x03,                   // body size=3
    0x00,                   // 0 local declarations
    0x01,                   // [0x3a] nop  <-- breakpoint
    0x0b,                   // [0x3b] end
]);

var module = new WebAssembly.Module(wasm);
var instance = new WebAssembly.Instance(module);

let test = instance.exports.test;

print("DEBUGGER_READY");
let iteration = 0;
for (;;) {
    test();
    iteration += 1;
    if (iteration % 1e6 == 0)
        print("iteration=", iteration);
}

// b 0x400000000000003a
