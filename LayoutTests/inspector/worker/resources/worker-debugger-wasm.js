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

instantiateWasm(wasmBytes);
addEventListener("message", ({data}) => {
    if (data === "instantiate")
        instantiateWasm(wasmBytes);
    else if (data === "instantiate-source-map")
        instantiateWasm(appendStringCustomSection(wasmBytes, "sourceMappingURL", "../../debugger/wasm/resources/source-map.json"));
});
postMessage("ready");
