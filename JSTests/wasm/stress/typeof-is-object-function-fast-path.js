//@ skip unless $isWasmPlatform
//@ runDefaultWasm()
//@ run("baseline", "--useDFGJIT=0", "--thresholdForJITAfterWarmUp=10", "--thresholdForJITSoon=10")
//@ run("lol", "--useDFGJIT=0", "--useLOLJIT=1", "--thresholdForJITAfterWarmUp=10", "--thresholdForJITSoon=10")

function shouldBe(actual, expected, message) {
    if (actual !== expected)
        throw new Error(message + ": bad value " + String(actual) + ", expected " + String(expected));
}
noInline(shouldBe);

function isObject(v) { return typeof v === "object"; }
noInline(isObject);

function isFunction(v) { return typeof v === "function"; }
noInline(isFunction);

let instance = new WebAssembly.Instance(new WebAssembly.Module(new Uint8Array([
    0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
    0x01, 0x04, 0x01, 0x60, 0x00, 0x00,
    0x03, 0x02, 0x01, 0x00,
    0x07, 0x05, 0x01, 0x01, 0x66, 0x00, 0x00,
    0x0a, 0x04, 0x01, 0x02, 0x00, 0x0b,
])));

const values = [
    ["wasm function", instance.exports.f, "function"],
    ["wasm instance", instance, "object"],
    ["wasm exports", instance.exports, "object"],
    ["WebAssembly.Module", WebAssembly.Module, "function"],
];

for (let i = 0; i < Math.max(50, testLoopCount / 100); ++i) {
    for (let [name, v, type] of values) {
        shouldBe(isObject(v), type === "object", name);
        shouldBe(isFunction(v), type === "function", name);
    }
}
