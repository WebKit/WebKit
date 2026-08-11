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

    assert.throws(() => new WebAssembly.Module(builder.WebAssembly().get()), WebAssembly.CompileError, "WebAssembly.Module doesn't parse at byte 24: Element section for Table 0 exceeds available Table 0");
}

{
    // Bad table index.
    const builder = new Builder()
        .Type().End()
        .Function().End()
        .Table()
            .Table({element: "funcref", initial: 20})
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

    assert.throws(() => new WebAssembly.Module(builder.WebAssembly().get()), WebAssembly.CompileError, "WebAssembly.Module doesn't parse at byte 31: Element section for Table 1 exceeds available Table 1");
}

{
    // Overflow table maximum size.
    const builder = new Builder()
        .Type().End()
        .Function().End()
        .Table()
            .Table({element: "funcref", initial: 20, maximum: 20})
        .End()
        .Element()
            .Element({offset: 19, functionIndices: [0, 0]})
        .End()
        .Code()
            .Function("foo", {params: ["i32"], ret: "i32"})
                .GetLocal(0)
                .I32Const(42)
                .I32Add()
                .Return()
            .End()
        .End();

    const module = new WebAssembly.Module(builder.WebAssembly().get());
    assert.throws(() => new WebAssembly.Instance(module), WebAssembly.RuntimeError, "Element is trying to set an out of bounds table index");
}

{
    // Overflow table maximum size.
    const builder = new Builder()
        .Type().End()
        .Function().End()
        .Table()
            .Table({element: "funcref", initial: 20, maximum: 20})
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

    const module = new WebAssembly.Module(builder.WebAssembly().get());
    assert.throws(() => new WebAssembly.Instance(module), WebAssembly.RuntimeError, "Element is trying to set an out of bounds table index");
}

{
    // Overflow function index space.
    const builder = new Builder()
        .Type().End()
        .Function().End()
        .Table()
            .Table({element: "funcref", initial: 20, maximum: 20})
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

    assert.throws(() => new WebAssembly.Module(builder.WebAssembly().get()), WebAssembly.CompileError, "WebAssembly.Module doesn't parse at byte 38: Element section's 0th element's 2th index is 1 which exceeds the function index space size of 1 (evaluating 'new WebAssembly.Module(builder.WebAssembly().get())')");
}

{
    function badInstantiation(actualTable, errorType, msg) {
        // Overflow function index space.
        const builder = new Builder()
            .Type().End()
            .Import()
                .Table("imp", "table", {element: "funcref", initial: 19}) // unspecified maximum.
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
        const table = new WebAssembly.Table({element: "funcref", initial: i});
        badInstantiation(table, WebAssembly.RuntimeError, "Element is trying to set an out of bounds table index (evaluating 'new WebAssembly.Instance(module, {imp: {table: actualTable}})')");
    }
}

{
    function makeModule() {
        const builder = new Builder()
            .Type().End()
            .Import()
                .Table("imp", "table", {element: "funcref", initial: 19}) // unspecified maximum.
                .Global().I32("imp", "global", "immutable").End()
            .End()
            .Function().End()
            .Element()
                .Element({offset: {op: "get_global", initValue: 0}, functionIndices: [0]})
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
        return new WebAssembly.Module(bin);
    }

    function test(i) {
        const table = new WebAssembly.Table({element: "funcref", initial: 19});
        const global = i;
        const module = makeModule();
        const instance = new WebAssembly.Instance(module, {imp: {table, global}});
        for (let j = 0; j < 19; j++) {
            if (j === i)
                assert.eq(table.get(j)(i*2), i*2 + 42);
            else
                assert.throws(() => table.get(j)(i*2), TypeError, "table.get(j) is not a function. (In 'table.get(j)(i*2)', 'table.get(j)' is null)");
        }
    }
    for (let i = 0; i < 19; i++)
        test(i);

    assert.throws(() => test(19), Error, "Element is trying to set an out of bounds table index");
}

{
    function badModule() {
        const builder = new Builder()
            .Type().End()
            .Import()
                .Table("imp", "table", {element: "funcref", initial: 19}) // unspecified maximum.
                .Global().F32("imp", "global", "immutable").End()
            .End()
            .Function().End()
            .Element()
                .Element({offset: {op: "get_global", initValue: 0}, functionIndices: [0]})
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
        return new WebAssembly.Module(bin);
    }

    assert.throws(() => badModule(), WebAssembly.CompileError, "WebAssembly.Module doesn't parse at byte 58: Element init_expr must produce an i32");
}
