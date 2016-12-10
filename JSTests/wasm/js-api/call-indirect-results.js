import * as assert from '../assert.js';
import Builder from '../Builder.js';

const wasmModuleWhichImportJS = () => {
    const builder = (new Builder())
        .Type().End()
        .Import()
            .Function("imp", "func", { params: ["i32"], ret: "i32" })
        .End()
        .Function().End()
        .Export()
            .Function("changeCounter")
        .End()
        .Code()
            .Function("changeCounter", { params: ["i32", "i32"], ret: "i32" })
                .I32Const(42)
                .GetLocal(0)
                .I32Add()
                .GetLocal(1)
                .CallIndirect(0, 0) // Calls func(param[0] + 42).
                .I32Const(0)
                .CallIndirect(0, 0) // Calls func(param[0] + 42).
            .End()
        .End();
    const bin = builder.WebAssembly().get();
    const module = new WebAssembly.Module(bin);
    return module;
};


(function MonomorphicImport() {
    let counter = 0;
    const counterSetter = v => counter = v;
    const module = wasmModuleWhichImportJS();
    const instance = new WebAssembly.Instance(module, { imp: { func: counterSetter } });
    for (let i = 0; i < 4096; ++i) {
        // Invoke this a bunch of times to make sure the IC in the wasm -> JS stub works correctly.
        instance.exports.changeCounter(i, 0);
        assert.eq(counter, i + 42);
    }
})();
