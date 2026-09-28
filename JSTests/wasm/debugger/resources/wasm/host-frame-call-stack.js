// A wasm export called from a C++ builtin -- JSON.stringify invoking it as toJSON -- so a
// host-function frame, which has no CodeBlock, sits between it and the JS driver. No JSPI.
var wasm = new Uint8Array([
    // [0x00] WASM header
    0x00, 0x61, 0x73, 0x6d, // magic
    0x01, 0x00, 0x00, 0x00, // version

    // [0x08] Type section: 1 function type
    0x01, 0x05,             // section id=1, size=5
    0x01,                   // 1 type
    0x60, 0x00, 0x01, 0x7f, // Type 0: (func [] -> [i32])

    // [0x0f] Import section: the suspending JS function
    0x02, 0x12,             // section id=2, size=18
    0x01,                   // 1 import
    0x03, 0x65, 0x6e, 0x76, // module name "env" (length=3)
    0x0a, 0x67, 0x65, 0x74, 0x5f, 0x6e, 0x75, 0x6d, 0x62, 0x65, 0x72, // "get_number" (length=10)
    0x00,                   // import kind: function
    0x00,                   // type index 0

    // [0x23] Function section: 1 function
    0x03, 0x02,             // section id=3, size=2
    0x01,                   // 1 function
    0x00,                   // func[1] uses type 0

    // [0x27] Export section: "entry"
    0x07, 0x09,             // section id=7, size=9
    0x01,                   // 1 export
    0x05, 0x65, 0x6e, 0x74, 0x72, 0x79, // "entry" (length=5)
    0x00,                   // export kind: function
    0x01,                   // func index 1

    // [0x32] Code section
    0x0a, 0x0c,             // section id=10, size=12
    0x01,                   // 1 function body
    0x0a,                   // body size=10
    0x00,                   // 0 local declarations
    // func[1] <entry> begins at 0x36; first instruction at 0x37
    0x41, 0x14,             // [0x37] i32.const 20
    0x10, 0x00,             // [0x39] call 0 <env.get_number>  -- suspends here
    0x41, 0x1e,             // [0x3b] i32.const 30
    0x6a,                   // [0x3d] i32.add
    0x6a,                   // [0x3e] i32.add
    0x0b,                   // [0x3f] end
]);

var module = new WebAssembly.Module(wasm);

var instance = new WebAssembly.Instance(module, {
    env: { get_number: function () { return 42; } }
});

// stringify is implemented in C++, so its frame is the one with no CodeBlock.
var obj = { toJSON: instance.exports.entry };

print("DEBUGGER_READY");

for (;;) {
    if (JSON.stringify(obj) !== "92") {
        print("FAIL: expected 92");
        break;
    }
}
