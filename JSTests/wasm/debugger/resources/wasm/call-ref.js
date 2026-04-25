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
    0x00, 0x01,             // import kind: function, type index 1

    // [0x22] Function section: 2 functions
    0x03, 0x03,             // section id=3, size=3
    0x02,                   // 2 functions
    0x01,                   // function 1 (wasm_func): type 1 ([] -> [i32])
    0x00,                   // function 2 (test): type 0 ([] -> [])

    // [0x27] Export section: export function 2 as "test"
    0x07, 0x08,             // section id=7, size=8
    0x01,                   // 1 export
    0x04, 0x74, 0x65, 0x73, 0x74, // name "test" (length=4)
    0x00, 0x02,             // export kind: function, function index 2

    // [0x31] Element section: declare functions for ref.func
    0x09, 0x06,             // section id=9, size=6
    0x01,                   // 1 element segment
    0x03,                   // flags=3 (declarative mode)
    0x00,                   // element kind: funcref
    0x02,                   // 2 elements
    0x01,                   // element 0: function index 1 (wasm_func)
    0x00,                   // element 1: function index 0 (js_func)

    // [0x39] Code section
    0x0a, 0x13,             // section id=10, size=19
    0x02,                   // 2 functions

    // [0x3c] Function 1 (wasm_func): returns constant 42
    0x04,                   // function body size=4
    0x00,                   // 0 local declarations
    0x41, 0x2a,             // [0x3e] i32.const 42
    0x0b,                   // [0x40] end

    // [0x41] Function 2 (test): calls wasm and JS functions via call_ref
    0x0c,                   // function body size=12
    0x00,                   // 0 local declarations
    0xd2, 0x01,             // [0x43] ref.func 1 (wasm_func)
    0x14, 0x01,             // [0x45] call_ref type 1
    0x1a,                   // [0x47] drop
    0xd2, 0x00,             // [0x48] ref.func 0 (js_func)
    0x14, 0x01,             // [0x4a] call_ref type 1
    0x1a,                   // [0x4c] drop
    0x0b                    // [0x4d] end
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
