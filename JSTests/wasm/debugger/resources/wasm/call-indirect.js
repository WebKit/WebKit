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
    0x00,                   // function 1: type 0 ([] -> [])
    0x01,                   // function 2: type 1 ([] -> [i32])

    // [0x27] Table section: for call_indirect
    0x04, 0x04,             // section id=4, size=4
    0x01,                   // 1 table
    0x70,                   // element type: funcref
    0x00, 0x02,             // limits: min=2

    // [0x2d] Export section: export function 1 as "test"
    0x07, 0x08,             // section id=7, size=8
    0x01,                   // 1 export
    0x04, 0x74, 0x65, 0x73, 0x74, // name "test" (length=4)
    0x00, 0x01,             // export kind: function, function index 1

    // [0x37] Element section: populate table
    0x09, 0x08,             // section id=9, size=8
    0x01,                   // 1 element segment
    0x00,                   // flags=0 (active, table 0)
    0x41, 0x00, 0x0b,       // offset expression: i32.const 0, end
    0x02,                   // 2 elements
    0x02,                   // element 0: function index 2 (wasm_func)
    0x00,                   // element 1: function index 0 (js_func)

    // [0x41] Code section
    0x0a, 0x15,             // section id=10, size=21
    0x02,                   // 2 functions

    // [0x44] Function 1 (test): calls via call_indirect
    0x0e,                   // function body size=14
    0x00,                   // 0 local declarations
    0x41, 0x00,             // [0x46] i32.const 0 (table index 0 = wasm)
    0x11, 0x01, 0x00,       // [0x48] call_indirect type=1, table=0
    0x1a,                   // [0x4b] drop
    0x41, 0x01,             // [0x4c] i32.const 1 (table index 1 = js)
    0x11, 0x01, 0x00,       // [0x4e] call_indirect type=1, table=0
    0x1a,                   // [0x51] drop
    0x0b,                   // [0x52] end

    // [0x53] Function 2 (wasm_func): returns constant 42
    0x04,                   // function body size=4
    0x00,                   // 0 local declarations
    0x41, 0x2a,             // [0x55] i32.const 42
    0x0b                    // [0x57] end
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
