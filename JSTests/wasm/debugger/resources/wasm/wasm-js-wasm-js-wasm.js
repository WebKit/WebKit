// Test case for WASM→JS→WASM→JS→WASM triple-interleaved call stack.
// Exercises two WasmToJS crossings, verifying that WasmToJSIPIntReturnPCSlot is read
// from the correct WasmToJS trampoline frame at each crossing.
//
// Call chain: JS main → test_outer[W] → js_mid_outer[JS] → test_mid[W] → js_mid_inner[JS] → wasm_inner[W]
//
// Function index mapping:
//   0: imported js.mid_outer (JS)
//   1: imported js.mid_inner (JS)
//   2: test_outer  (WASM, exported) — calls mid_outer (import 0)
//   3: test_mid    (WASM, exported) — calls mid_inner (import 1)
//   4: wasm_inner  (WASM, exported) — nop, breakpoint target
//
// Expected bt at 0x400000000000006c:
//   frame #0: 0x400000000000006c   (wasm_inner nop — breakpoint)
//   frame #1: 0xc000000000000000   (JS js_mid_inner frame)
//   frame #2: 0x4000000000000069   (test_mid return PC, after 'call mid_inner' at 0x67 — from WasmToJSIPIntReturnPCSlot in WasmToJS frame)
//   frame #3: 0xc000000000000000   (JS js_mid_outer frame)
//   frame #4: 0x4000000000000064   (test_outer return PC, after 'call mid_outer' at 0x62 — from WasmToJSIPIntReturnPCSlot in WasmToJS frame)
//   frame #5: 0xc000000000000000   (JS main loop)
var wasm = new Uint8Array([
    // [0x00] WASM header
    0x00, 0x61, 0x73, 0x6d, // magic
    0x01, 0x00, 0x00, 0x00, // version

    // [0x08] Type section: 1 type (func [] -> [])
    0x01, 0x04,             // section id=1, size=4
    0x01,                   // 1 type
    0x60, 0x00, 0x00,       // type 0: (func [] -> [])

    // [0x0e] Import section: js.mid_outer, js.mid_inner
    0x02, 0x1f,             // section id=2, size=31
    0x02,                   // 2 imports
    0x02, 0x6a, 0x73,       // module name "js" (length=2)
    0x09, 0x6d, 0x69, 0x64, 0x5f, 0x6f, 0x75, 0x74, 0x65, 0x72, // field "mid_outer" (length=9)
    0x00, 0x00,             // import kind: function, type index 0
    0x02, 0x6a, 0x73,       // module name "js" (length=2)
    0x09, 0x6d, 0x69, 0x64, 0x5f, 0x69, 0x6e, 0x6e, 0x65, 0x72, // field "mid_inner" (length=9)
    0x00, 0x00,             // import kind: function, type index 0

    // [0x2f] Function section: 3 WASM functions (indices 2–4, all type 0)
    0x03, 0x04,             // section id=3, size=4
    0x03,                   // 3 functions
    0x00, 0x00, 0x00,       // all type 0

    // [0x35] Export section: test_outer (func 2), test_mid (func 3), wasm_inner (func 4)
    0x07, 0x26,             // section id=7, size=38
    0x03,                   // 3 exports
    0x0a, 0x74, 0x65, 0x73, 0x74, 0x5f, 0x6f, 0x75, 0x74, 0x65, 0x72, // name "test_outer" (length=10)
    0x00, 0x02,             // kind: function, index 2
    0x08, 0x74, 0x65, 0x73, 0x74, 0x5f, 0x6d, 0x69, 0x64, // name "test_mid" (length=8)
    0x00, 0x03,             // kind: function, index 3
    0x0a, 0x77, 0x61, 0x73, 0x6d, 0x5f, 0x69, 0x6e, 0x6e, 0x65, 0x72, // name "wasm_inner" (length=10)
    0x00, 0x04,             // kind: function, index 4

    // [0x5d] Code section
    0x0a, 0x0f,             // section id=10, size=15
    0x03,                   // 3 function bodies

    // [0x60] Function 2 (test_outer): calls mid_outer (import 0)
    0x04,                   // body size=4
    0x00,                   // 0 local declarations
    0x10, 0x00,             // [0x62] call 0 (mid_outer)
    0x0b,                   // [0x64] end  ← return PC in test_outer

    // [0x65] Function 3 (test_mid): calls mid_inner (import 1)
    0x04,                   // body size=4
    0x00,                   // 0 local declarations
    0x10, 0x01,             // [0x67] call 1 (mid_inner)
    0x0b,                   // [0x69] end  ← return PC in test_mid

    // [0x6a] Function 4 (wasm_inner): breakpoint target
    0x03,                   // body size=3
    0x00,                   // 0 local declarations
    0x01,                   // [0x6c] nop  <-- breakpoint
    0x0b,                   // [0x6d] end
]);

var test_mid_func;
var wasm_inner_func;

function js_mid_inner() {
    wasm_inner_func();
}

function js_mid_outer() {
    test_mid_func();
}

var module = new WebAssembly.Module(wasm);
var instance = new WebAssembly.Instance(module, { js: { mid_outer: js_mid_outer, mid_inner: js_mid_inner } });

test_mid_func = instance.exports.test_mid;
wasm_inner_func = instance.exports.wasm_inner;
let test_outer = instance.exports.test_outer;

print("DEBUGGER_READY");
let iteration = 0;
for (;;) {
    test_outer();
    iteration += 1;
    if (iteration % 1e6 == 0)
        print("iteration=", iteration);
}

// b 0x400000000000006c
