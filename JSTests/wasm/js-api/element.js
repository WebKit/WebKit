import Builder from '../Builder.js';
import * as assert from '../assert.js';

{
    // Bad element section b/c no Table section/import.
    const builder = new Builder()
        .Type().End()
        .Function().End()
        .Element()
            .Element({tableIndex: 0, offset: 0, functionIndices: [0]})
        .End()
        .Code()
            .Function("foo", {params: ["i32"], ret: "i32"})
                .GetLocal(0)
                .I32Const(42)
                .I32Add()
                .Return()
            .End()
        .End();

    assert.throws(() => new WebAssembly.Module(builder.WebAssembly().get()), WebAssembly.CompileError, "WebAssembly.Module doesn't parse at byte 22 / 41: Element section expects a Table to be present (evaluating 'new WebAssembly.Module(builder.WebAssembly().get())')");
}

{
    // Bad table index.
    const builder = new Builder()
        .Type().End()
        .Function().End()
        .Table()
            .Table({element: "anyfunc", initial: 20})
        .End()
        .Element()
            .Element({tableIndex: 1, offset: 0, functionIndices: [0]})
        .End()
        .Code()
            .Function("foo", {params: ["i32"], ret: "i32"})
                .GetLocal(0)
                .I32Const(42)
                .I32Add()
                .Return()
            .End()
        .End();

    assert.throws(() => new WebAssembly.Module(builder.WebAssembly().get()), WebAssembly.CompileError, "WebAssembly.Module doesn't parse at byte 30 / 47: Element section can only have one Table for now (evaluating 'new WebAssembly.Module(builder.WebAssembly().get())')");
}

{
    // Overflow table maximum size.
    const builder = new Builder()
        .Type().End()
        .Function().End()
        .Table()
            .Table({element: "anyfunc", initial: 20, maximum: 20})
        .End()
        .Element()
            .Element({offset: 19, functionIndices: [0, 1]})
        .End()
        .Code()
            .Function("foo", {params: ["i32"], ret: "i32"})
                .GetLocal(0)
                .I32Const(42)
                .I32Add()
                .Return()
            .End()
        .End();

    assert.throws(() => new WebAssembly.Module(builder.WebAssembly().get()), WebAssembly.CompileError, "WebAssembly.Module doesn't parse at byte 35 / 49: Element section's 0th element writes to index 20 which exceeds the maximum 20 (evaluating 'new WebAssembly.Module(builder.WebAssembly().get())')");
}

{
    // Overflow table maximum size.
    const builder = new Builder()
        .Type().End()
        .Function().End()
        .Table()
            .Table({element: "anyfunc", initial: 20, maximum: 20})
        .End()
        .Element()
            .Element({offset: 20, functionIndices: [0]})
        .End()
        .Code()
            .Function("foo", {params: ["i32"], ret: "i32"})
                .GetLocal(0)
                .I32Const(42)
                .I32Add()
                .Return()
            .End()
        .End();

    assert.throws(() => new WebAssembly.Module(builder.WebAssembly().get()), WebAssembly.CompileError, "WebAssembly.Module doesn't parse at byte 35 / 48: Element section's 0th element writes to index 20 which exceeds the maximum 20 (evaluating 'new WebAssembly.Module(builder.WebAssembly().get())')");
}

{
    // Overflow function index space.
    const builder = new Builder()
        .Type().End()
        .Function().End()
        .Table()
            .Table({element: "anyfunc", initial: 20, maximum: 20})
        .End()
        .Element()
            .Element({offset: 0, functionIndices: [0, 0, 1]})
        .End()
        .Code()
            .Function("foo", {params: ["i32"], ret: "i32"})
                .GetLocal(0)
                .I32Const(42)
                .I32Add()
                .Return()
            .End()
        .End();

    assert.throws(() => new WebAssembly.Module(builder.WebAssembly().get()), WebAssembly.CompileError, "WebAssembly.Module doesn't parse at byte 38 / 50: Element section's 0th element's 2th index is 1 which exceeds the function index space size of 1 (evaluating 'new WebAssembly.Module(builder.WebAssembly().get())')");
}

{
    function badInstantiation(actualTable, errorType, msg) {
        // Overflow function index space.
        const builder = new Builder()
            .Type().End()
            .Import()
                .Table("imp", "table", {element: "anyfunc", initial: 19}) // unspecified maximum.
            .End()
            .Function().End()
            .Element()
                .Element({offset: 19, functionIndices: [0, 0, 0, 0, 0]})
            .End()
            .Code()
                .Function("foo", {params: ["i32"], ret: "i32"})
                    .GetLocal(0)
                    .I32Const(42)
                    .I32Add()
                    .Return()
                .End()
            .End();

        const bin = builder.WebAssembly().get();
        const module = new WebAssembly.Module(bin);
        assert.throws(() => new WebAssembly.Instance(module, {imp: {table: actualTable}}), errorType, msg);
    }

    for (let i = 19; i < 19 + 5; i++) {
        const table = new WebAssembly.Table({element: "anyfunc", initial: i});
        badInstantiation(table, RangeError, "Element is trying to set an out of bounds table index");
    }
}
