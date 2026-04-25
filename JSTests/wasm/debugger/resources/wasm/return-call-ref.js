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

    // [0x30] Element section: declare functions for ref.func
    0x09, 0x07,             // section id=9, size=7
    0x01,                   // 1 element segment
    0x03,                   // flags=3 (declarative mode)
    0x00,                   // element kind: funcref
    0x03,                   // 3 elements
    0x04,                   // element 0: function index 4 (wasm_func)
    0x00,                   // element 1: function index 0 (js_func)
    0x01,                   // element 2: function index 1 (caller, for completeness)

    // [0x39] Code section
    0x0a,                   // section id=10
    0x1f,                   // section size=31
    0x04,                   // 4 functions

    // [0x3c] Function 1 (caller): calls test_wasm_tail and test_js_tail
    0x0a,                   // function body size=10
    0x00,                   // 0 local declarations
    0x10, 0x02,             // [0x3e] call 2 (test_wasm_tail)
    0x10, 0x03,             // [0x40] call 3 (test_js_tail)
    0x6a,                   // [0x42] i32.add
    0x41, 0x01,             // [0x43] i32.const 1
    0x6a,                   // [0x45] i32.add
    0x0b,                   // [0x46] end

    // [0x47] Function 2 (test_wasm_tail): tail calls wasm_func via ref.func
    0x06,                   // function body size=6
    0x00,                   // 0 local declarations
    0xd2, 0x04,             // [0x49] ref.func 4 (wasm_func)
    0x15, 0x00,             // [0x4b] return_call_ref type 0
    0x0b,                   // [0x4d] end

    // [0x4e] Function 3 (test_js_tail): tail calls js_func via ref.func
    0x06,                   // function body size=6
    0x00,                   // 0 local declarations
    0xd2, 0x00,             // [0x50] ref.func 0 (js_func)
    0x15, 0x00,             // [0x52] return_call_ref type 0
    0x0b,                   // [0x54] end

    // [0x55] Function 4 (wasm_func): returns constant 42
    0x04,                   // function body size=4
    0x00,                   // 0 local declarations
    0x41, 0x2a,             // [0x57] i32.const 42
    0x0b                    // [0x59] end
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
