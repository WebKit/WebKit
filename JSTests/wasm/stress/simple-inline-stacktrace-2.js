//@ skip if !$isWasmPlatform
//@ skip if $memoryLimited
//@ runDefault("--maximumWasmDepthForInlining=10", "--maximumWasmCalleeSizeForInlining=10000000", "--maximumWasmCallerSizeForInlining=10000000", "--useBBQJIT=0")
var wasm_code = read('simple-inline-stacktrace.wasm', 'binary')
var wasm_module = new WebAssembly.Module(wasm_code);
var wasm_instance = new WebAssembly.Instance(wasm_module, { a: { doThrow: () => { throw new Error() } } });
var f = wasm_instance.exports.main;
for (let i = 0; i < 10000; ++i) {
    try {
        f()
    } catch (e) {
        let str = e.stack.toString()
        let trace = str.split('\n')
        let expected = ["*", "wasm-stub@[wasm code]", "<?>.wasm-function[11]@[wasm code]",
            "<?>.wasm-function[17]@[wasm code]", "<?>.wasm-function[16]@[wasm code]", "<?>.wasm-function[15]@[wasm code]",
            "<?>.wasm-function[14]@[wasm code]", "<?>.wasm-function[13]@[wasm code]", "<?>.wasm-function[12]@[wasm code]",
            "<?>.wasm-function[18]@[wasm code]", "wasm-stub@[wasm code]", "*"]
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
if (mem != 10000)
    throw "Expected 10000, got " + mem