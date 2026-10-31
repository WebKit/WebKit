//@ requireOptions("--useWasmTailCalls=true")

// f tail-calls g in another instance, which makes IPInt insert a restore frame
// whose return PC points back into the JS-to-wasm entry code.
const module = new WebAssembly.Module(read("tail-call-cross-instance-jit-cage.wasm", "binary"));
const other = new WebAssembly.Instance(module, { m: { g: () => 0 } });
const { f } = new WebAssembly.Instance(module, { m: { g: other.exports.g } }).exports;

for (let i = 0; i < wasmTestLoopCount; ++i) {
    if (f() !== 42)
        throw new Error("bad result");
}
