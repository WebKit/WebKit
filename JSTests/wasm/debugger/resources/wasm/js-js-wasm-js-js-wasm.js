// Test case for deeply interleaved JS->JS->WASM->JS->JS->WASM call stack.
// Exercises multiple JS frames on each side (DFG inlined + non-inlined) together
// with the WasmToJS trampoline peek and JSToWasm trampoline skip.
//
// Call chain: JS main -> js_a -> js_b -> test[WASM] -> callback -> js_c -> wasm_inner[WASM]
//
// Function index mapping:
//   0: imported js.callback (JS)
//   1: test       (WASM, exported) — calls js.callback
//   2: wasm_inner (WASM, exported) — nop, breakpoint target
//
// Expected bt at 0x4000000000000045:
//   frame #0: 0x4000000000000045   (wasm_inner nop — breakpoint)
//   frame #1: 0xc000000000000000   (js_c)
//   frame #2: 0xc000000000000000   (callback)
//   frame #3: 0x4000000000000042   (test call-site return PC — after 'call 0' at 0x40)
//   frame #4: 0xc000000000000000   (js_b)
//   frame #5: 0xc000000000000000   (js_a)
//   frame #6: 0xc000000000000000   (JS main loop)
var wasm = new Uint8Array([
    // [0x00] WASM header
    0x00, 0x61, 0x73, 0x6d, // magic
    0x01, 0x00, 0x00, 0x00, // version

    // [0x08] Type section: 1 type
    0x01, 0x04,             // section id=1, size=4
    0x01,                   // 1 type
    0x60, 0x00, 0x00,       // type 0: (func [] -> [])

    // [0x0e] Import section: js.callback
    0x02, 0x0f,             // section id=2, size=15
    0x01,                   // 1 import
    0x02, 0x6a, 0x73,       // module name "js" (length=2)
    0x08, 0x63, 0x61, 0x6c, 0x6c, 0x62, 0x61, 0x63, 0x6b, // field name "callback" (length=8)
    0x00, 0x00,             // import kind: function, type index 0

    // [0x1f] Function section: 2 WASM functions (both type 0)
    0x03, 0x03,             // section id=3, size=3
    0x02,                   // 2 functions
    0x00, 0x00,             // both type 0

    // [0x24] Export section: test (func 1), wasm_inner (func 2)
    0x07, 0x15,             // section id=7, size=21
    0x02,                   // 2 exports
    0x04, 0x74, 0x65, 0x73, 0x74,                                   // name "test" (length=4)
    0x00, 0x01,             // kind: function, index 1
    0x0a, 0x77, 0x61, 0x73, 0x6d, 0x5f, 0x69, 0x6e, 0x6e, 0x65, 0x72, // name "wasm_inner" (length=10)
    0x00, 0x02,             // kind: function, index 2

    // [0x3b] Code section
    0x0a, 0x0a,             // section id=10, size=10
    0x02,                   // 2 functions

    // [0x3e] Function 1 (test): calls js.callback
    0x04,                   // body size=4
    0x00,                   // 0 local declarations
    0x10, 0x00,             // [0x40] call 0 (js.callback)
    0x0b,                   // [0x42] end

    // [0x43] Function 2 (wasm_inner): breakpoint target
    0x03,                   // body size=3
    0x00,                   // 0 local declarations
    0x01,                   // [0x45] nop  <-- breakpoint
    0x0b,                   // [0x46] end
]);

var wasm_inner_func;

function js_c() {
    wasm_inner_func();
}

function callback() {
    js_c();
}

function js_b() {
    test();
}

function js_a() {
    js_b();
}

var module = new WebAssembly.Module(wasm);
var instance = new WebAssembly.Instance(module, { js: { callback } });

wasm_inner_func = instance.exports.wasm_inner;
let test = instance.exports.test;

print("DEBUGGER_READY");
let iteration = 0;
for (;;) {
    js_a();
    iteration += 1;
    if (iteration % 1e6 == 0)
        print("iteration=", iteration);
}

// b 0x4000000000000045
