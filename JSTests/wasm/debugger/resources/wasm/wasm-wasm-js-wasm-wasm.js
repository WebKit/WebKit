// Test case for WASM→WASM→JS→WASM→WASM interleaved call stack.
// Exercises IPInt callee tracking across a WASM→WASM hop on each side of a JS boundary.
//
// Call chain: JS main → test_a[W] → test_b[W] → mid_callback[JS] → test_c[W] → wasm_leaf[W]
//
// Function index mapping:
//   0: imported js.mid_callback (JS)
//   1: test_a      (WASM, exported) — calls test_b (func 2)
//   2: test_b      (WASM)           — calls mid_callback (import 0)
//   3: test_c      (WASM, exported) — calls wasm_leaf (func 4)
//   4: wasm_leaf   (WASM, exported) — nop, breakpoint target
//
// Expected bt at 0x400000000000005f:
//   frame #0: 0x400000000000005f   (wasm_leaf nop — breakpoint)
//   frame #1: 0x400000000000005c   (test_c return PC, after 'call wasm_leaf' at 0x5a)
//   frame #2: 0xc000000000000000   (JS mid_callback frame)
//   frame #3: 0x4000000000000057   (test_b return PC, after 'call mid_callback' at 0x55 — from WasmToJSIPIntReturnPCSlot in WasmToJS frame)
//   frame #4: 0x4000000000000052   (test_a return PC, after 'call test_b' at 0x50 — read from test_b−8)
//   frame #5: 0xc000000000000000   (JS main loop)
var wasm = new Uint8Array([
    // [0x00] WASM header
    0x00, 0x61, 0x73, 0x6d, // magic
    0x01, 0x00, 0x00, 0x00, // version

    // [0x08] Type section: 1 type (func [] -> [])
    0x01, 0x04,             // section id=1, size=4
    0x01,                   // 1 type
    0x60, 0x00, 0x00,       // type 0: (func [] -> [])

    // [0x0e] Import section: js.mid_callback
    0x02, 0x13,             // section id=2, size=19
    0x01,                   // 1 import
    0x02, 0x6a, 0x73,       // module name "js" (length=2)
    0x0c, 0x6d, 0x69, 0x64, 0x5f, 0x63, 0x61, 0x6c, 0x6c, 0x62, 0x61, 0x63, 0x6b, // field "mid_callback" (length=12)
    0x00, 0x00,             // import kind: function, type index 0

    // [0x23] Function section: 4 WASM functions (indices 1–4, all type 0)
    0x03, 0x05,             // section id=3, size=5
    0x04,                   // 4 functions
    0x00, 0x00, 0x00, 0x00, // all type 0

    // [0x2a] Export section: test_a (func 1), test_c (func 3), wasm_leaf (func 4)
    0x07, 0x1f,             // section id=7, size=31
    0x03,                   // 3 exports
    0x06, 0x74, 0x65, 0x73, 0x74, 0x5f, 0x61,  // name "test_a" (length=6)
    0x00, 0x01,             // kind: function, index 1
    0x06, 0x74, 0x65, 0x73, 0x74, 0x5f, 0x63,  // name "test_c" (length=6)
    0x00, 0x03,             // kind: function, index 3
    0x09, 0x77, 0x61, 0x73, 0x6d, 0x5f, 0x6c, 0x65, 0x61, 0x66, // name "wasm_leaf" (length=9)
    0x00, 0x04,             // kind: function, index 4

    // [0x4b] Code section
    0x0a, 0x14,             // section id=10, size=20
    0x04,                   // 4 function bodies

    // [0x4e] Function 1 (test_a): calls test_b (func 2)
    0x04,                   // body size=4
    0x00,                   // 0 local declarations
    0x10, 0x02,             // [0x50] call 2 (test_b)
    0x0b,                   // [0x52] end  ← return PC in test_a

    // [0x53] Function 2 (test_b): calls mid_callback (import 0)
    0x04,                   // body size=4
    0x00,                   // 0 local declarations
    0x10, 0x00,             // [0x55] call 0 (mid_callback)
    0x0b,                   // [0x57] end  ← return PC in test_b

    // [0x58] Function 3 (test_c): calls wasm_leaf (func 4)
    0x04,                   // body size=4
    0x00,                   // 0 local declarations
    0x10, 0x04,             // [0x5a] call 4 (wasm_leaf)
    0x0b,                   // [0x5c] end  ← return PC in test_c

    // [0x5d] Function 4 (wasm_leaf): breakpoint target
    0x03,                   // body size=3
    0x00,                   // 0 local declarations
    0x01,                   // [0x5f] nop  <-- breakpoint
    0x0b,                   // [0x60] end
]);

var test_c_func;

function mid_callback() {
    test_c_func();
}

var module = new WebAssembly.Module(wasm);
var instance = new WebAssembly.Instance(module, { js: { mid_callback } });

test_c_func = instance.exports.test_c;
let test_a = instance.exports.test_a;

print("DEBUGGER_READY");
let iteration = 0;
for (;;) {
    test_a();
    iteration += 1;
    if (iteration % 1e6 == 0)
        print("iteration=", iteration);
}

// b 0x400000000000005f
