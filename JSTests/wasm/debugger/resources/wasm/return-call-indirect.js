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

    // [0x26] Table section: for call_indirect
    0x04, 0x04,             // section id=4, size=4
    0x01,                   // 1 table
    0x70,                   // funcref type
    0x00, 0x02,             // limits: min=2, no max

    // [0x2c] Export section: export function 1 as "test"
    0x07, 0x08, 0x01, 0x04, 0x74, 0x65, 0x73, 0x74, 0x00, 0x01,

    // [0x36] Element section: initialize table with functions
    0x09, 0x08,             // section id=9, size=8
    0x01,                   // 1 element segment
    0x00,                   // flags: active, table index 0
    0x41, 0x00, 0x0b,       // offset: i32.const 0, end
    0x02,                   // 2 function indices
    0x04,                   // table[0] = function 4 (wasm_func)
    0x00,                   // table[1] = function 0 (js_func)

    // [0x40] Code section
    0x0a,                   // section id=10
    0x21,                   // section size=33
    0x04,                   // 4 functions

    // [0x43] Function 1 (caller): calls test_wasm_tail and test_js_tail
    0x0a,                   // function body size=10
    0x00,                   // 0 local declarations
    0x10, 0x02,             // [0x45] call 2 (test_wasm_tail)
    0x10, 0x03,             // [0x47] call 3 (test_js_tail)
    0x6a,                   // [0x49] i32.add
    0x41, 0x01,             // [0x4a] i32.const 1
    0x6a,                   // [0x4c] i32.add
    0x0b,                   // [0x4d] end

    // [0x4e] Function 2 (test_wasm_tail): tail calls wasm_func via table[0]
    0x07,                   // function body size=7
    0x00,                   // 0 local declarations
    0x41, 0x00,             // [0x50] i32.const 0 (table[0] = wasm_func)
    0x13, 0x00, 0x00,       // [0x52] return_call_indirect type 0, table 0
    0x0b,                   // [0x55] end

    // [0x56] Function 3 (test_js_tail): tail calls js_func via table[1]
    0x07,                   // function body size=7
    0x00,                   // 0 local declarations
    0x41, 0x01,             // [0x58] i32.const 1 (table[1] = js_func)
    0x13, 0x00, 0x00,       // [0x5a] return_call_indirect type 0, table 0
    0x0b,                   // [0x5d] end

    // [0x5e] Function 4 (wasm_func): returns constant 42
    0x04,                   // function body size=4
    0x00,                   // 0 local declarations
    0x41, 0x2a,             // [0x60] i32.const 42
    0x0b                    // [0x62] end
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
