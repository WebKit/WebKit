importScripts("../../../resources/wasm-builder.js", "../../debugger/wasm/resources/test-modules.js");

let wasmBytes = createAddWasmBytes();
let wasmInstances = [];

function encodeULEB128(value)
{
    let bytes = [];
    do {
        let byte = value & 0x7f;
        value >>>= 7;
        if (value)
            byte |= 0x80;
        bytes.push(byte);
    } while (value);
    return bytes;
}

function appendStringCustomSection(bytes, name, value)
{
    let encoder = new TextEncoder;
    name = encoder.encode(name);
    value = encoder.encode(value);
    let payload = [...encodeULEB128(name.length), ...name, ...encodeULEB128(value.length), ...value];
    return new Uint8Array([...bytes, 0, ...encodeULEB128(payload.length), ...payload]);
}

function instantiateWasm(bytes)
{
    let wasmModule = new WebAssembly.Module(bytes);
    wasmInstances.push(new WebAssembly.Instance(wasmModule));
}

async function instantiateStreamingWasm()
{
    let base64 = "AGFzbQEAAAABBwFgAn9/AX8DAgEABQMBAAEHEAIDYWRkAAAGbWVtb3J5AgAKCQEHACAAIAFqCw==";
    let {instance} = await WebAssembly.instantiateStreaming(fetch("data:application/wasm;base64," + base64));
    wasmInstances.push(instance);
}

if (location.search !== "?no-initial-instance")
    instantiateWasm(wasmBytes);
addEventListener("message", ({data}) => {
    if (data === "instantiate")
        instantiateWasm(wasmBytes);
    else if (data === "instantiate-source-map")
        instantiateWasm(appendStringCustomSection(wasmBytes, "sourceMappingURL", "../../debugger/wasm/resources/source-map.json"));
    else if (data === "instantiate-streaming")
        instantiateStreamingWasm().then(() => postMessage("instantiated-streaming"));
    else if (data === "invoke")
        postMessage(wasmInstances[wasmInstances.length - 1].exports.add(2, 3));
});
postMessage("ready");
