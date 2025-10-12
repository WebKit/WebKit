//@ skip if $memoryLimited
//@ runDefault("--maximumWasmDepthForInlining=10", "--maximumWasmCalleeSizeForInlining=10000000", "--maximumWasmCallerSizeForInlining=10000000", "--useBBQJIT=0")
var wasm_code = read('simple-inline-stacktrace.wasm', 'binary')
var wasm_module = new WebAssembly.Module(wasm_code);
var wasm_instance = new WebAssembly.Instance(wasm_module, { a: { doThrow: () => { throw new Error() } } });
var f = wasm_instance.exports.main;
for (let i = 0; i < wasmTestLoopCount; ++i) {
    try {
        f()
    } catch (e) {
        let str = e.stack.toString()
        let trace = str.split('\n')
        let expected = ["*", "g@wasm-function[11]",
        "f@wasm-function[17]", "e@wasm-function[16]", "d@wasm-function[15]",
        "c@wasm-function[14]", "b@wasm-function[13]", "a@wasm-function[12]",
        "main@wasm-function[18]", "*"]
        if (trace.length != expected.length)
            throw "unexpected length"
        for (let i = 0; i < trace.length; ++i) {
            if (expected[i] == "*")
                continue
            if (expected[i] != trace[i].trim())
                throw "mismatch at " + i
        }
    }
}

let mem = new Int32Array(wasm_instance.exports.mem.buffer)[0]
if (mem != wasmTestLoopCount)
    throw `Expected ${wasmTestLoopCount}, got ${mem}`;
