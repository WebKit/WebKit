import Builder from '../Builder.js';
import * as assert from '../assert.js';

function assertBadBinary(builder, str) {
    const bin = builder.WebAssembly().get();
    let threw = false;
    try {
        new WebAssembly.Module(bin);
    } catch(e) {
        threw = true;
        assert.truthy(e.toString().indexOf(str) !== -1);
        assert.truthy(e instanceof WebAssembly.CompileError);
    }
    assert.truthy(threw);
}

const badElementSectionString = "couldn't parse section Element";

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

    assertBadBinary(builder, badElementSectionString);
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

    assertBadBinary(builder, badElementSectionString);
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

    assertBadBinary(builder, badElementSectionString);
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

    assertBadBinary(builder, badElementSectionString);
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

    assertBadBinary(builder, badElementSectionString);
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
