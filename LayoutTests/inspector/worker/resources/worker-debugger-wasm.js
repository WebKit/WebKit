importScripts("../../../resources/wasm-builder.js", "../../debugger/wasm/resources/test-modules.js");

let wasmBytes = createAddWasmBytes();
let wasmInstances = [];

function instantiateWasm(bytes)
{
    let wasmModule = new WebAssembly.Module(bytes);
    wasmInstances.push(new WebAssembly.Instance(wasmModule));
}

instantiateWasm(wasmBytes);
addEventListener("message", ({data}) => {
    if (data === "instantiate")
        instantiateWasm(wasmBytes);
});
postMessage("ready");
