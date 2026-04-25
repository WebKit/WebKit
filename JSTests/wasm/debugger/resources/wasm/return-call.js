const wasm = new Uint8Array([
    // [0x00] WASM header
    0x00, 0x61, 0x73, 0x6d, // magic
    0x01, 0x00, 0x00, 0x00, // version

    // [0x08] Type section: 1 function type
    0x01, 0x05,             // section id=1, size=5
    0x01,                   // 1 type
    0x60, 0x00, 0x01, 0x7f, // Type 0: (func [] -> [i32])

    // [0x0f] Import section: import JS function
    0x02, 0x0e,             // section id=2, size=14
    0x01,                   // 1 import
    0x02, 0x6a, 0x73,       // module name "js" (length=2)
    0x07, 0x6a, 0x73, 0x5f, 0x66, 0x75, 0x6e, 0x63, // field name "js_func" (length=7)
    0x00,                   // import kind: function
    0x00,                   // type index 0

    // [0x1f] Function section: 4 functions
    0x03, 0x05,             // section id=3, size=5
    0x04,                   // 4 functions
    0x00,                   // function 1 (caller): type 0 ([] -> [i32])
    0x00,                   // function 2 (test_wasm_tail): type 0 ([] -> [i32])
    0x00,                   // function 3 (test_js_tail): type 0 ([] -> [i32])
    0x00,                   // function 4 (wasm_func): type 0 ([] -> [i32])

    // [0x26] Export section: export function 1 as "test"
    0x07, 0x08, 0x01, 0x04, 0x74, 0x65, 0x73, 0x74, 0x00, 0x01,

    // [0x30] Code section
    0x0a,                   // section id=10
    0x1f,                   // section size=31
    0x04,                   // 4 functions

    // [0x33] Function 1 (caller): calls test_wasm_tail and test_js_tail
    0x0a,                   // function body size=10
    0x00,                   // 0 local declarations
    0x10, 0x02,             // [0x35] call 2 (test_wasm_tail)
    0x10, 0x03,             // [0x37] call 3 (test_js_tail)
    0x6a,                   // [0x39] i32.add
    0x41, 0x01,             // [0x3a] i32.const 1
    0x6a,                   // [0x3c] i32.add
    0x0b,                   // [0x3d] end

    // [0x3e] Function 2 (test_wasm_tail): tail calls wasm_func
    0x06,                   // function body size=6
    0x00,                   // 0 local declarations
    0x12, 0x04,             // [0x40] return_call 4 (wasm_func)
    0x41, 0x00,             // [0x42] i32.const 0 (unreachable)
    0x0b,                   // [0x44] end

    // [0x45] Function 3 (test_js_tail): tail calls js_func
    0x06,                   // function body size=6
    0x00,                   // 0 local declarations
    0x12, 0x00,             // [0x47] return_call 0 (js_func)
    0x41, 0x00,             // [0x49] i32.const 0 (unreachable)
    0x0b,                   // [0x4b] end

    // [0x4c] Function 4 (wasm_func): returns constant 42
    0x04,                   // function body size=4
    0x00,                   // 0 local declarations
    0x41, 0x2a,             // [0x4e] i32.const 42
    0x0b                    // [0x50] end
]);

function js_func() {
    return 99;
}

var module = new WebAssembly.Module(wasm);
var instance_step = new WebAssembly.Instance(module, {
    js: {
        js_func: js_func
    }
});

let test = instance_step.exports.test;

print("DEBUGGER_READY");
let iteration = 0;
for (; ;) {
    let result = test();
    iteration += 1;
    if (iteration % 1e6 == 0)
        print("iteration=", iteration, "result=", result);
}
