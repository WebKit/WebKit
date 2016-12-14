import * as assert from '../assert.js';
import Builder from '../Builder.js';

{
    // Test init from non-import.
    const builder = new Builder();

    builder.Type().End()
        .Function().End()
        .Global().GetGlobal("i32", 0, "immutable").End()
        .Export()
            .Function("getGlobal")
            .Global("global", 1)
        .End()
        .Code()

        .Function("getGlobal", { params: [], ret: "i32" })
        .GetGlobal(1)
        .End()

        .End()

    const bin = builder.WebAssembly();
    bin.trim();

    let passed = false;
    try {
        const module = new WebAssembly.Module(bin.get());
    } catch (e) {
        if (e.message === "couldn't parse section Global: Global declarations (evaluating 'new WebAssembly.Module(bin.get())')")
            passed = true;
    }
    assert.truthy(passed);
}


{
    // Test import mutable.
    const builder = new Builder();

    builder.Type().End()
        .Import()
            .Global().I32("imp", "global", "mutable").End()
        .End()
        .Function().End()
        .Global().GetGlobal("i32", 0, "immutable").End()
        .Export()
            .Function("getGlobal")
            .Global("global", 1)
        .End()
        .Code()

        .Function("getGlobal", { params: [], ret: "i32" })
        .GetGlobal(1)
        .End()

        .End()

    const bin = builder.WebAssembly();
    bin.trim();

    let passed = false;
    try {
        const module = new WebAssembly.Module(bin.get());
    } catch (e) {
        if (e.message === "couldn't parse section Import: Import declarations (evaluating 'new WebAssembly.Module(bin.get())')")
            passed = true;
    }
    assert.truthy(passed);
}

{
    // Test export mutable.
    const builder = new Builder();

    builder.Type().End()
        .Function().End()
        .Global().I32(0, "mutable").End()
        .Export()
            .Function("setGlobal")
            .Global("global", 0)
        .End()
        .Code()

        .Function("setGlobal", { params: [], ret: "i32" })
        .GetGlobal(1)
        .End()

        .End()

    const bin = builder.WebAssembly();
    bin.trim();

    let passed = false;
    try {
        const module = new WebAssembly.Module(bin.get());
    } catch (e) {
        if (e.message === "couldn't parse section Export: Exports (evaluating 'new WebAssembly.Module(bin.get())')")
            passed = true;
    }
    assert.truthy(passed);
}

{
    // Test set immutable.
    const builder = new Builder();

    builder.Type().End()
        .Function().End()
        .Global().I32(0, "immutable").End()
        .Export()
            .Function("setGlobal")
            .Global("global", 0)
        .End()
        .Code()

        .Function("setGlobal", { params: [], ret: "i32" })
        .I32Const(0)
        .SetGlobal(0)
        .End()

        .End()

    const bin = builder.WebAssembly();
    bin.trim();

    let passed = false;
    try {
        const module = new WebAssembly.Module(bin.get());
    } catch (e) {
        if (e.message === "Attempt to store to immutable global. (evaluating 'new WebAssembly.Module(bin.get())')")
            passed = true;
    }
    assert.truthy(passed);
}


{
    // Test set non-existant global.
    const builder = new Builder();

    builder.Type().End()
        .Function().End()
        .Global().I32(0, "immutable").End()
        .Export()
            .Function("setGlobal")
            .Global("global", 0)
        .End()
        .Code()

        .Function("setGlobal", { params: [], ret: "i32" })
        .I32Const(0)
        .SetGlobal(1)
        .End()

        .End()

    const bin = builder.WebAssembly();
    bin.trim();

    let passed = false;
    try {
        const module = new WebAssembly.Module(bin.get());
    } catch (e) {
        if (e.message === "Attempt to use unknown global. (evaluating 'new WebAssembly.Module(bin.get())')")
            passed = true;
    }
    assert.truthy(passed);
}


{
    // Test set to incorrect type
    const builder = new Builder();

    builder.Type().End()
        .Function().End()
        .Global().F32(0, "mutable").End()
        .Export()
            .Function("setGlobal")
        .End()
        .Code()

        .Function("setGlobal", { params: [], ret: "i32" })
        .I32Const(0)
        .SetGlobal(0)
        .End()

        .End()

    const bin = builder.WebAssembly();
    bin.trim();

    let passed = false;
    try {
        const module = new WebAssembly.Module(bin.get());
    } catch (e) {
        if (e.message === "Attempt to set global with type: F32 with a variable of type: I32 (evaluating 'new WebAssembly.Module(bin.get())')")
            passed = true;
    }
    assert.truthy(passed);
}

for ( let imp of [undefined, null, {}, () => {}, "number", new Number(4)]) {
    // Test import non-number.
    const builder = new Builder();

    builder.Type().End()
        .Import()
            .Global().I32("imp", "global", "immutable").End()
        .End()
        .Function().End()
        .Global().GetGlobal("i32", 0, "immutable").End()
        .Export()
            .Function("getGlobal")
            .Global("global", 1)
        .End()
        .Code()

        .Function("getGlobal", { params: [], ret: "i32" })
        .GetGlobal(1)
        .End()

        .End()

    const bin = builder.WebAssembly();
    bin.trim();

    let passed = false;
    const module = new WebAssembly.Module(bin.get());
    try {
        new WebAssembly.Instance(module, { imp: { global: imp } });
    } catch (e) {
        if (e.message === "imported global must be a number (evaluating 'new WebAssembly.Instance(module, { imp: { global: imp } })')")
            passed = true;
    }
    assert.truthy(passed);
}
