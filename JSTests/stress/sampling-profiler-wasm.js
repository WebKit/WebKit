//@ skip if $architecture == "arm"
//@ runDefault

if (platformSupportsSamplingProfiler() && $vm.isWasmSupported()) {
    load("./sampling-profiler/samplingProfiler.js", "caller relative");
    let buf = read("./sampling-profiler/loop.wasm", "binary");
    let module = new WebAssembly.Module(buf);
    let instance = new WebAssembly.Instance(module);
    var wasmEntry = function() {
        return instance.exports.loop(10000000);
    };
    runTest(wasmEntry, ["<?>.wasm-function[0]", "wasm-stub", "wasmEntry"]);
}
