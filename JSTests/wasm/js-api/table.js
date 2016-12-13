import Builder from '../Builder.js';
import * as assert from '../assert.js';

const badTableString = "couldn't parse section Table";
const badImportString = "couldn't parse section Import";
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

{
    const builder = new Builder()
        .Type().End()
        .Import()
            .Table("imp", "table", {initial: 20, element: "anyfunc"})
        .End()
        .Function().End()
        .Table()
            .Table({initial: 20, maximum: 30, element: "anyfunc"})
        .End()
        .Code()
        .End();
    assertBadBinary(builder, badTableString);
}

{
    const builder = new Builder()
        .Type().End()
        .Function().End()
        .Table()
            .Table({initial: 20, maximum: 30, element: "anyfunc"})
            .Table({initial: 20, maximum: 30, element: "anyfunc"})
        .End()
        .Code()
        .End();
    assertBadBinary(builder, badTableString);
}

{
    const builder = new Builder()
        .Type().End()
        .Function().End()
        .Export()
            .Function("foo")
        .End()
        .Code()
            .Function("foo", {params: ["i32"]})
                .GetLocal(0)
                .CallIndirect(0, 0)
            .End()
        .End();
    assertBadBinary(builder, "call_indirect is only valid when a table is defined or imported");
}

{
    const builder = new Builder()
        .Type().End()
        .Function().End()
        .Table()
            .Table({initial:20, element:"anyfunc"})
        .End()
        .Export()
            .Function("foo")
        .End()
        .Code()
            .Function("foo", {params: ["i32"]})
                .GetLocal(0)
                .CallIndirect(0, 1)
            .End()
        .End();
    assertBadBinary(builder, "call_indirect 'reserved' varuint1 must be 0x0");
}

function assertBadTable(tableDescription) {
    const builder = new Builder()
        .Type().End()
        .Function().End()
        .Table()
            .Table(tableDescription)
        .End()
        .Code()
        .End();
    assertBadBinary(builder, badTableString);
}

function assertBadTableImport(tableDescription) {
    const builder = new Builder()
        .Type().End()
        .Import()
            .Table("imp", "table", tableDescription)
        .End()
        .Function().End()
        .Code()
        .End();
    assertBadBinary(builder, badImportString);
}

{
    let badDescriptions = [
        {initial: 10, element: "i32"},
        {initial: 10, element: "f32"},
        {initial: 10, element: "f64"},
        {initial: 10, element: "i64"},
        {initial: 10, maximum: 20, element: "i32"},
        {initial: 10, maximum: 20, element: "f32"},
        {initial: 10, maximum: 20, element: "f64"},
        {initial: 10, maximum: 20, element: "i64"},

        {initial: 10, maximum: 9, element: "anyfunc"},
        {initial: 1, maximum: 0, element: "anyfunc"},
        {initial: 2**32 - 1, maximum: 2**32 - 2, element: "anyfunc"},
        {initial: 2**31, element: "anyfunc"},
    ];

    for (const d of badDescriptions) {
        assertBadTable(d);
        assertBadTableImport(d);
    }
}

{
    const builder = new Builder()
        .Type().End()
        .Import()
            .Table("imp", "table", {initial: 20, element: "anyfunc"})
            .Table("imp", "table", {initial: 20, element: "anyfunc"})
        .End()
        .Function().End()
        .Code()
        .End();
    assertBadBinary(builder, badImportString);
}


{
    function assertBadTableInstance(tableDescription, table, message) {
        const builder = new Builder()
            .Type().End()
            .Import()
                .Table("imp", "table", tableDescription)
            .End()
            .Function().End()
            .Code()
            .End();

        let threw = false;
        const module = new WebAssembly.Module(builder.WebAssembly().get());
        try {
            new WebAssembly.Instance(module, {imp: {table}});
        } catch (e) {
            assert.eq(e.toString(), message);
            threw = true;
        }
        assert.truthy(threw);
    }

    const badTables = [
        [{initial: 100, maximum:100, element:"anyfunc"}, new WebAssembly.Table({initial:100, element: "anyfunc"}), "TypeError: Table import does not have a 'maximum' but the module requires that it does"],
        [{initial: 100, maximum:100, element:"anyfunc"}, new WebAssembly.Table({initial:100, maximum:101, element: "anyfunc"}), "TypeError: Imported Table's 'maximum' is larger than the module's expected 'maximum'"],
        [{initial: 100, element:"anyfunc"}, new WebAssembly.Table({initial:10, element: "anyfunc"}), "TypeError: Table import provided an 'initial' that is too small"],
        [{initial: 10, element:"anyfunc"}, new WebAssembly.Table({initial:9, element: "anyfunc"}), "TypeError: Table import provided an 'initial' that is too small"],
    ];
    for (const [d, t, m] of badTables) {
        assertBadTableInstance(d, t, m);
    }
}

{
    {
        const table = new WebAssembly.Table({element: "anyfunc", initial: 20, maximum: 30});
        table.grow(30);
        assert.throws(() => table.grow(31), TypeError, "WebAssembly.Table.prototype.grow could not grow the table");
        assert.throws(() => table.grow(29), TypeError, "WebAssembly.Table.prototype.grow could not grow the table");
    }

    {
        const table = new WebAssembly.Table({element: "anyfunc", initial: 20});
        assert.throws(() => table.grow({valueOf() { return 19; }}), TypeError, "WebAssembly.Table.prototype.grow could not grow the table");
        let called = false;
        table.grow({valueOf() { called = true; return 21; }});
        assert.truthy(called);
    }

    {
        const table = new WebAssembly.Table({element: "anyfunc", initial: 20});
        assert.throws(() => table.get(20), RangeError, "WebAssembly.Table.prototype.get expects an integer less than the size of the table");
        for (let i = 0; i < 20; i++)
            assert.eq(table.get(i), null);
    }

    {
        const table = new WebAssembly.Table({element: "anyfunc", initial: 20});
        assert.throws(() => table.set(20, null), RangeError, "WebAssembly.Table.prototype.set expects an integer less than the size of the table");
        for (let i = 0; i < 20; i++)
            table.set(i, null);
    }

    {
        // This should not throw
        new WebAssembly.Table({initial: 2**20, maximum: 2**32 - 1, element: "anyfunc"});
    }
}

{

    function assertBadTable(table) {
        const builder = new Builder()
            .Type().End()
            .Import()
                .Table("imp", "table", {initial: 25, element: "anyfunc"})
            .End()
            .Function().End()
            .Code()
            .End();
        const module = new WebAssembly.Module(builder.WebAssembly().get());
        assert.throws(() => new WebAssembly.Instance(module, {imp: {table}}), TypeError, "Table import is not an instance of WebAssembly.Table");
    }
    assertBadTable(25);
    assertBadTable(new Object);
    assertBadTable([]);
    assertBadTable(new WebAssembly.Memory({initial:1}));
}
