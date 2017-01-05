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

    assert.throws(() => new WebAssembly.Module(bin.get()), WebAssembly.CompileError, "WebAssembly.Module doesn't parse at byte 26 / 59: get_global's index 0 exceeds the number of imports 0 (evaluating 'new WebAssembly.Module(bin.get())')");
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

    assert.throws(() => new WebAssembly.Module(bin.get()), WebAssembly.CompileError, "WebAssembly.Module doesn't parse at byte 32 / 76: Mutable Globals aren't supported (evaluating 'new WebAssembly.Module(bin.get())')");
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

    assert.throws(() => new WebAssembly.Module(bin.get()), WebAssembly.CompileError, "WebAssembly.Module doesn't parse at byte 51 / 59: 1th Export isn't immutable, named 'global' (evaluating 'new WebAssembly.Module(bin.get())')");
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

    assert.throws(() => new WebAssembly.Module(bin.get()), WebAssembly.CompileError, "WebAssembly.Module doesn't validate: set_global 0 is immutable (evaluating 'new WebAssembly.Module(bin.get())')");
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

    assert.throws(() => new WebAssembly.Module(bin.get()), WebAssembly.CompileError, "WebAssembly.Module doesn't validate: set_global 1 of unknown global, limit is 1 (evaluating 'new WebAssembly.Module(bin.get())')");
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

    assert.throws(() => new WebAssembly.Module(bin.get()), WebAssembly.CompileError, "WebAssembly.Module doesn't validate: set_global 0 with type F32 with a variable of type I32 (evaluating 'new WebAssembly.Module(bin.get())')");
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

    const module = new WebAssembly.Module(bin.get());
    assert.throws(() => new WebAssembly.Instance(module, { imp: { global: imp } }), WebAssembly.LinkError, "imported global must be a number (evaluating 'new WebAssembly.Instance(module, { imp: { global: imp } })')");
}
