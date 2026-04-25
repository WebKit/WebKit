import * as assert from '../assert.js';
import Builder from '../Builder.js';

function createInternalGlobalModule() {
    const builder = new Builder();

    builder.Type().End()
        .Function().End()
        .Global().I32(5, "mutable").End()
        .Export()
            .Function("getGlobal")
            .Function("setGlobal")
        .End()
        .Code()

        // GetGlobal
        .Function("getGlobal", { params: [], ret: "i32" })
            .GetGlobal(0)
        .End()

        // SetGlobal
        .Function("setGlobal", { params: ["i32"] })
            .GetLocal(0)
            .SetGlobal(0)
        .End()

        .End()

    const bin = builder.WebAssembly();
    bin.trim();

    const module = new WebAssembly.Module(bin.get());
    const instance = new WebAssembly.Instance(module);
    assert.eq(instance.exports.getGlobal(), 5);
    instance.exports.setGlobal(3);
    assert.eq(instance.exports.getGlobal(), 3);
}

createInternalGlobalModule();
