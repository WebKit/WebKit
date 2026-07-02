var wasm = new Uint8Array([
    // [0x00] WASM header
    0x00, 0x61, 0x73, 0x6d, // magic
    0x01, 0x00, 0x00, 0x00, // version

    // [0x08] Type section: 2 function types
    0x01, 0x08,             // section id=1, size=8
    0x02,                   // 2 types
    0x60, 0x00, 0x00,       // Type 0: (func [] -> [])
    0x60, 0x00, 0x01, 0x7f, // Type 1: (func [] -> [i32])

    // [0x12] Import section: import JS function
    0x02, 0x0e,             // section id=2, size=14
    0x01,                   // 1 import
    0x02, 0x6a, 0x73,       // module name "js" (length=2)
    0x07, 0x6a, 0x73, 0x5f, 0x66, 0x75, 0x6e, 0x63, // field name "js_func" (length=7)
    0x00,                   // import kind: function
    0x01,                   // type index 1

    // [0x22] Function section: 2 functions
    0x03, 0x03,             // section id=3, size=3
    0x02,                   // 2 functions
    0x00,                   // function 1: type 0 ([] -> [])
    0x01,                   // function 2: type 1 ([] -> [i32])

    // [0x27] Export section: export function 1 as "test"
    0x07, 0x08, 0x01, 0x04, 0x74, 0x65, 0x73, 0x74, 0x00, 0x01,

    // [0x31] Code section
    0x0a,                   // section id=10
    0x0f,                   // section size=15
    0x02,                   // 2 functions

    // [0x34] Function 1 (test): calls function 2 (wasm) and function 0 (imported JS)
    0x08,                   // function body size=8
    0x00,                   // 0 local declarations
    0x10, 0x02,             // [0x36] call 2 (wasm func)
    0x1a,                   // [0x38] drop
    0x10, 0x00,             // [0x39] call 0 (js func)
    0x1a,                   // [0x3b] drop
    0x0b,                   // [0x3c] end

    // [0x3d] Function 2 (wasm_func): returns constant 42
    0x04,                   // function body size=4
    0x00,                   // 0 local declarations
    0x41, 0x2a,             // [0x3f] i32.const 42
    0x0b                    // [0x41] end
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
    test();
    iteration += 1;
    if (iteration % 1e6 == 0)
        print("iteration=", iteration);
}
