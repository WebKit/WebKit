//@ skip if $addressBits <= 32
//@ runDefaultWasm("--useBBQJIT=0", "--useConcurrentJIT=0", "--wasmInliningMaximumDepth=10", "--wasmInliningMaximumWasmCalleeSize=10000000", "--wasmInliningBudget=100000", "--thresholdForOMGOptimizeAfterWarmUp=0", "--thresholdForOMGOptimizeSoon=0")
//@ runDefaultWasm("--useBBQJIT=0", "--useConcurrentJIT=1", "--wasmInliningMaximumDepth=10", "--wasmInliningMaximumWasmCalleeSize=10000000", "--wasmInliningBudget=100000", "--thresholdForOMGOptimizeAfterWarmUp=0", "--thresholdForOMGOptimizeSoon=0")
var wasm_code = read('simple-inline-stacktrace-with-catch.wasm', 'binary');
var wasm_module = new WebAssembly.Module(wasm_code);
let throwCounter = 0;
let throwAt = 0;
var wasm_instance = new WebAssembly.Instance(wasm_module, { a: { doThrow: () => {
    if (throwCounter == throwAt)
        throw new Error();
    ++throwCounter;
} } });
var f = wasm_instance.exports.main;

function verifyStack(stack, e) {
    let trace = e.stack.toString().split('\n');
    let expected = ["*"];
    for (let i of stack)
        expected.push(`${i}@wasm-function[${i}]`);
    expected.push("*");

    if (trace.length != expected.length)
        throw "unexpected length, got:\n" + e.stack + "\nExpected:\n" + expected.join("\n");
    for (let i = 0; i < trace.length; ++i) {
        if (expected[i] == "*")
            continue;
        if (expected[i] != trace[i].trim())
            throw "mismatch at " + i + ", got:\n" + e.stack + "\nExpected:\n" + expected.join("\n");
    }
}

const cases = [
    { throwAt: 8, stack: [14, 15, 16] },
    { throwAt: 9, stack: [11, 12, 13, 14, 15, 16] },
    { throwAt: 11, stack: [14, 15, 16] },
];

const warmups = 20;
for (let c of cases) {
    for (let i = 0; i < warmups; ++i) {
        try {
            throwCounter = 0;
            throwAt = c.throwAt;
            f();
        } catch (e) {
            verifyStack(c.stack, e);
        }
    }
}
