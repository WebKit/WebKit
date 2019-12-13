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

    assert.throws(() => new WebAssembly.Module(bin.get()), WebAssembly.CompileError, "WebAssembly.Module doesn't parse at byte 26: get_global's index 0 exceeds the number of globals 0 (evaluating 'new WebAssembly.Module(bin.get())')");
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
    assert.throws(() => new WebAssembly.Module(bin.get()), WebAssembly.CompileError, `WebAssembly.Module doesn't parse at byte 43: get_global import kind index 0 is mutable  (evaluating 'new WebAssembly.Module(bin.get())')`);
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
        .GetGlobal(0)
        .End()

        .End()

    const bin = builder.WebAssembly();
    bin.trim();
    new WebAssembly.Module(bin.get());
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

    assert.throws(() => new WebAssembly.Module(bin.get()), WebAssembly.CompileError, "WebAssembly.Module doesn't validate: set_global 0 is immutable, in function at index 0 (evaluating 'new WebAssembly.Module(bin.get())')");
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

    assert.throws(() => new WebAssembly.Module(bin.get()), WebAssembly.CompileError, "WebAssembly.Module doesn't validate: set_global 1 of unknown global, limit is 1, in function at index 0 (evaluating 'new WebAssembly.Module(bin.get())')");
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

    assert.throws(() => new WebAssembly.Module(bin.get()), WebAssembly.CompileError, "WebAssembly.Module doesn't validate: set_global 0 with type F32 with a variable of type I32, in function at index 0 (evaluating 'new WebAssembly.Module(bin.get())')");
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
    assert.throws(() => new WebAssembly.Instance(module, { imp: { global: imp } }), WebAssembly.LinkError, "imported global imp:global must be a number (evaluating 'new WebAssembly.Instance(module, { imp: { global: imp } })')");
}

{
    const builder = new Builder()
        .Type().End()
        .Global().I64(0, "immutable").End()
        .Export()
            .Global("bigInt", 0)
        .End();
    const module = new WebAssembly.Module(builder.WebAssembly().get());
    let instance = new WebAssembly.Instance(module);
    assert.throws(() => instance.exports.bigInt.value, TypeError, "WebAssembly.Global.prototype.value does not work with i64 type");
}

{
    const builder = new Builder()
        .Type().End()
        .Import()
            .Global().I64("imp", "global", "immutable").End()
        .End()
        .Function().End()
        .Global().GetGlobal("i64", 0, "immutable").End();
    const module = new WebAssembly.Module(builder.WebAssembly().get());
    assert.throws(() => new WebAssembly.Instance(module, { imp: { global: undefined } }), WebAssembly.LinkError, "imported global imp:global cannot be an i64 (evaluating 'new WebAssembly.Instance(module, { imp: { global: undefined } })')");
}
