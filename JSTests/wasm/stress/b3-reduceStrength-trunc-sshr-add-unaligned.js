// Wasm module equivalent to:
// (module
//   (func (export "test") (param i64) (result i32)
//     local.get 0
//     i64.const 2048
//     i64.add
//     i64.const 12
//     i64.shr_s
//     i32.wrap_i64
//   )
// )
var wasm_code = new Uint8Array([
    0x00, 0x61, 0x73, 0x6d, // magic
    0x01, 0x00, 0x00, 0x00, // version

    // Type section
    0x01, 0x06,             // section id=1, size=6
    0x01,                   // 1 type
    0x60,                   // func type
    0x01, 0x7e,             // 1 param: i64
    0x01, 0x7f,             // 1 result: i32

    // Function section
    0x03, 0x02,             // section id=3, size=2
    0x01,                   // 1 function
    0x00,                   // type index 0

    // Export section
    0x07, 0x08,             // section id=7, size=8
    0x01,                   // 1 export
    0x04,                   // name length=4
    0x74, 0x65, 0x73, 0x74, // "test"
    0x00,                   // func export
    0x00,                   // func index 0

    // Code section
    0x0a, 0x0e,             // section id=10, size=14
    0x01,                   // 1 function body
    0x0c,                   // body size=12
    0x00,                   // 0 locals
    0x20, 0x00,             // local.get 0
    0x42, 0x80, 0x10,       // i64.const 2048 (signed LEB128)
    0x7c,                   // i64.add
    0x42, 0x0c,             // i64.const 12 (signed LEB128)
    0x87,                   // i64.shr_s
    0xa7,                   // i32.wrap_i64
    0x0b,                   // end
]);

var wasm_module = new WebAssembly.Module(wasm_code);
var wasm_instance = new WebAssembly.Instance(wasm_module);
var test = wasm_instance.exports.test;

function expected(a) {
    var result = (BigInt(a) + 2048n) >> 12n;
    return Number(result);
}

// Warm up to trigger OMG compilation.
for (var i = 0; i < 100; i++) {
    var result = test(2048n);
    if (result !== expected(2048))
        throw new Error("FAIL: test(2048) = " + result + ", expected " + expected(2048));
}

// Test various values after OMG compilation.
var testCases = [
    [0n, expected(0)],
    [2048n, expected(2048)],
    [4095n, expected(4095)],
    [4096n, expected(4096)],
    [100000n, expected(100000)],
    [-2048n, expected(-2048)],
    [-1n, expected(-1)],
    [1n, expected(1)],
];

for (var i = 0; i < testCases.length; i++) {
    var input = testCases[i][0];
    var exp = testCases[i][1];
    var result = test(input);
    if (result !== exp)
        throw new Error("FAIL: test(" + input + ") = " + result + ", expected " + exp);
}
