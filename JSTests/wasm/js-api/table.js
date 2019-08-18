import Builder from '../Builder.js';
import * as assert from '../assert.js';

{
    const builder = new Builder()
        .Type().End()
        .Import()
            .Table("imp", "table", {initial: 20, element: "funcref"})
        .End()
        .Function().End()
        .Table()
            .Table({initial: 20, maximum: 30, element: "funcref"})
        .End()
        .Code()
        .End();
    new WebAssembly.Module(builder.WebAssembly().get())
}

{
    const builder = new Builder()
        .Type().End()
        .Function().End()
        .Table()
            // Table count is zero.
        .End()
        .Code()
        .End();
    new WebAssembly.Module(builder.WebAssembly().get());
}

{
    const builder = new Builder()
        .Type().End()
        .Function().End()
        .Table()
            .Table({initial: 20, maximum: 30, element: "funcref"})
            .Table({initial: 20, maximum: 30, element: "funcref"})
        .End()
        .Code()
        .End();
    new WebAssembly.Module(builder.WebAssembly().get())
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
    assert.throws(() => new WebAssembly.Module(builder.WebAssembly().get()), WebAssembly.CompileError, "WebAssembly.Module doesn't parse at byte 4: call_indirect is only valid when a table is defined or imported, in function at index 0 (evaluating 'new WebAssembly.Module(builder.WebAssembly().get())')");
}

{
    const builder = new Builder()
        .Type().End()
        .Function().End()
        .Table()
            .Table({initial:20, element:"funcref"})
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
    assert.throws(() => new WebAssembly.Module(builder.WebAssembly().get()), WebAssembly.CompileError, "WebAssembly.Module doesn't parse at byte 6: call_indirect's table index 1 invalid, limit is 1, in function at index 0 (evaluating 'new WebAssembly.Module(builder.WebAssembly().get())')");
}

{
    const builder = new Builder()
        .Type().End()
        .Function().End()
        .Table()
            .Table({initial:20, element:"funcref"})
            .Table({initial:20, element:"funcref"})
        .End()
        .Export()
            .Function("foo")
        .End()
        .Code()
            .Function("foo", {params: ["i32"]})
                .GetLocal(0)
                .GetLocal(0)
                .CallIndirect(0, 1)
            .End()
        .End();
    new WebAssembly.Module(builder.WebAssembly().get())
}

{
    // Can't export an undefined table
    const builder = new Builder()
        .Type().End()
        .Function().End()
        .Export()
            .Table("foo", 0)
        .End()
        .Code()
        .End();
    assert.throws(() => new WebAssembly.Module(builder.WebAssembly().get()), WebAssembly.CompileError, "WebAssembly.Module doesn't parse at byte 23: can't export Table 0 there are 0 Tables");
}

{
    // Can't export a table at index 1.
    const builder = new Builder()
        .Type().End()
        .Function().End()
        .Table()
            .Table({initial: 20, maximum: 30, element: "funcref"})
        .End()
        .Export()
            .Table("foo", 1)
        .End()
        .Code()
        .End();
    assert.throws(() => new WebAssembly.Module(builder.WebAssembly().get()), WebAssembly.CompileError, "WebAssembly.Module doesn't parse at byte 30: can't export Table 1 there are 1 Tables");
}

function assertBadTable(tableDescription, message) {
    const builder = new Builder()
        .Type().End()
        .Function().End()
        .Table()
            .Table(tableDescription)
        .End()
        .Code()
        .End();
    assert.throws(() => new WebAssembly.Module(builder.WebAssembly().get()), WebAssembly.CompileError, message);
}

function assertBadTableImport(tableDescription, message) {
    const builder = new Builder()
        .Type().End()
        .Import()
            .Table("imp", "table", tableDescription)
        .End()
        .Function().End()
        .Code()
        .End();
    assert.throws(() => new WebAssembly.Module(builder.WebAssembly().get()), WebAssembly.CompileError, message);
}

{
    let badDescriptions = [
        [{initial: 10, element: "i32"},
         "WebAssembly.Module doesn't parse at byte 18: Table type should be funcref or anyref, got -1 (evaluating 'new WebAssembly.Module(builder.WebAssembly().get())')",
         "WebAssembly.Module doesn't parse at byte 26: Table type should be funcref or anyref, got -1 (evaluating 'new WebAssembly.Module(builder.WebAssembly().get())')"],
        [{initial: 10, element: "f32"},
         "WebAssembly.Module doesn't parse at byte 18: Table type should be funcref or anyref, got -3 (evaluating 'new WebAssembly.Module(builder.WebAssembly().get())')",
         "WebAssembly.Module doesn't parse at byte 26: Table type should be funcref or anyref, got -3 (evaluating 'new WebAssembly.Module(builder.WebAssembly().get())')"],
        [{initial: 10, element: "f64"},
         "WebAssembly.Module doesn't parse at byte 18: Table type should be funcref or anyref, got -4 (evaluating 'new WebAssembly.Module(builder.WebAssembly().get())')",
         "WebAssembly.Module doesn't parse at byte 26: Table type should be funcref or anyref, got -4 (evaluating 'new WebAssembly.Module(builder.WebAssembly().get())')"],
        [{initial: 10, element: "i64"},
         "WebAssembly.Module doesn't parse at byte 18: Table type should be funcref or anyref, got -2 (evaluating 'new WebAssembly.Module(builder.WebAssembly().get())')",
         "WebAssembly.Module doesn't parse at byte 26: Table type should be funcref or anyref, got -2 (evaluating 'new WebAssembly.Module(builder.WebAssembly().get())')"],
        [{initial: 10, maximum: 20, element: "i32"},
         "WebAssembly.Module doesn't parse at byte 18: Table type should be funcref or anyref, got -1 (evaluating 'new WebAssembly.Module(builder.WebAssembly().get())')",
         "WebAssembly.Module doesn't parse at byte 26: Table type should be funcref or anyref, got -1 (evaluating 'new WebAssembly.Module(builder.WebAssembly().get())')"],
        [{initial: 10, maximum: 20, element: "f32"},
         "WebAssembly.Module doesn't parse at byte 18: Table type should be funcref or anyref, got -3 (evaluating 'new WebAssembly.Module(builder.WebAssembly().get())')",
         "WebAssembly.Module doesn't parse at byte 26: Table type should be funcref or anyref, got -3 (evaluating 'new WebAssembly.Module(builder.WebAssembly().get())')"],
        [{initial: 10, maximum: 20, element: "f64"},
         "WebAssembly.Module doesn't parse at byte 18: Table type should be funcref or anyref, got -4 (evaluating 'new WebAssembly.Module(builder.WebAssembly().get())')",
         "WebAssembly.Module doesn't parse at byte 26: Table type should be funcref or anyref, got -4 (evaluating 'new WebAssembly.Module(builder.WebAssembly().get())')"],
        [{initial: 10, maximum: 20, element: "i64"},
         "WebAssembly.Module doesn't parse at byte 18: Table type should be funcref or anyref, got -2 (evaluating 'new WebAssembly.Module(builder.WebAssembly().get())')",
         "WebAssembly.Module doesn't parse at byte 26: Table type should be funcref or anyref, got -2 (evaluating 'new WebAssembly.Module(builder.WebAssembly().get())')"],

        [{initial: 10, maximum: 9, element: "funcref"},
         "WebAssembly.Module doesn't parse at byte 21: resizable limits has an initial page count of 10 which is greater than its maximum 9 (evaluating 'new WebAssembly.Module(builder.WebAssembly().get())')",
         "WebAssembly.Module doesn't parse at byte 29: resizable limits has an initial page count of 10 which is greater than its maximum 9 (evaluating 'new WebAssembly.Module(builder.WebAssembly().get())')"],
        [{initial: 1, maximum: 0, element: "funcref"},
         "WebAssembly.Module doesn't parse at byte 21: resizable limits has an initial page count of 1 which is greater than its maximum 0 (evaluating 'new WebAssembly.Module(builder.WebAssembly().get())')",
         "WebAssembly.Module doesn't parse at byte 29: resizable limits has an initial page count of 1 which is greater than its maximum 0 (evaluating 'new WebAssembly.Module(builder.WebAssembly().get())')"],
        [{initial: 2**32 - 1, maximum: 2**32 - 2, element: "funcref"},
         "WebAssembly.Module doesn't parse at byte 29: resizable limits has an initial page count of 4294967295 which is greater than its maximum 4294967294 (evaluating 'new WebAssembly.Module(builder.WebAssembly().get())')",
         "WebAssembly.Module doesn't parse at byte 37: resizable limits has an initial page count of 4294967295 which is greater than its maximum 4294967294 (evaluating 'new WebAssembly.Module(builder.WebAssembly().get())')"],
        [{initial: 2**31, element: "funcref"},
         "WebAssembly.Module doesn't parse at byte 24: Table's initial page count of 2147483648 is too big, maximum 10000000 (evaluating 'new WebAssembly.Module(builder.WebAssembly().get())')",
         "WebAssembly.Module doesn't parse at byte 32: Table's initial page count of 2147483648 is too big, maximum 10000000 (evaluating 'new WebAssembly.Module(builder.WebAssembly().get())')"],
    ];

    for (const d of badDescriptions) {
        assertBadTable(d[0], d[1]);
        assertBadTableImport(d[0], d[2]);
    }
}

{
    const builder = new Builder()
        .Type().End()
        .Import()
            .Table("imp", "table", {initial: 20, element: "funcref"})
            .Table("imp", "table", {initial: 20, element: "funcref"})
        .End()
        .Function().End()
        .Code()
        .End();
    new WebAssembly.Module(builder.WebAssembly().get())
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
        const module = new WebAssembly.Module(builder.WebAssembly().get());
        assert.throws(() => new WebAssembly.Instance(module, {imp: {table}}), WebAssembly.LinkError, message);
    }

    const badTables = [
        [{initial: 100, maximum:100, element:"funcref"}, new WebAssembly.Table({initial:100, element: "funcref"}), "Table import imp:table does not have a 'maximum' but the module requires that it does (evaluating 'new WebAssembly.Instance(module, {imp: {table}})')"],
        [{initial: 100, maximum:100, element:"funcref"}, new WebAssembly.Table({initial:100, maximum:101, element: "funcref"}), "Imported Table imp:table 'maximum' is larger than the module's expected 'maximum' (evaluating 'new WebAssembly.Instance(module, {imp: {table}})')"],
        [{initial: 100, element:"funcref"}, new WebAssembly.Table({initial:10, element: "funcref"}), "Table import imp:table provided an 'initial' that is too small (evaluating 'new WebAssembly.Instance(module, {imp: {table}})')"],
        [{initial: 10, element:"funcref"}, new WebAssembly.Table({initial:9, element: "funcref"}), "Table import imp:table provided an 'initial' that is too small (evaluating 'new WebAssembly.Instance(module, {imp: {table}})')"],
    ];
    for (const [d, t, m] of badTables) {
        assertBadTableInstance(d, t, m);
    }
}

assert.throws(() => WebAssembly.Table.prototype.grow(undefined), TypeError, `expected |this| value to be an instance of WebAssembly.Table`);

{
    {
        const table = new WebAssembly.Table({element: "funcref", initial: 20, maximum: 30});
        assert.eq(20, table.grow(0));
        assert.eq(20, table.length);
        assert.eq(20, table.grow(1));
        assert.eq(21, table.length);
    }

    {
        const table = new WebAssembly.Table({element: "funcref", initial: 20, maximum: 30});
        assert.eq(20, table.grow(10));
        assert.eq(30, table.grow(0));
        assert.throws(() => table.grow(1), RangeError, "WebAssembly.Table.prototype.grow could not grow the table");
    }

    {
        const table = new WebAssembly.Table({element: "funcref", initial: 20});
        let called = false;
        table.grow({valueOf() { called = true; return 42; }});
        assert.truthy(called);
        assert.eq(62, table.length);
    }

    {
        const table = new WebAssembly.Table({element: "funcref", initial: 20});
        assert.throws(() => table.get(20), RangeError, "WebAssembly.Table.prototype.get expects an integer less than the length of the table");
        for (let i = 0; i < 20; i++)
            assert.eq(table.get(i), null);
    }

    {
        const table = new WebAssembly.Table({element: "funcref", initial: 20});
        assert.throws(() => table.set(20, null), RangeError, "WebAssembly.Table.prototype.set expects an integer less than the length of the table");
        for (let i = 0; i < 20; i++)
            table.set(i, null);
    }

    {
        // This should not throw
        new WebAssembly.Table({initial: 2**20, maximum: 2**32 - 1, element: "funcref"});
    }
}

{

    function assertBadTable(table) {
        const builder = new Builder()
            .Type().End()
            .Import()
                .Table("imp", "table", {initial: 25, element: "funcref"})
            .End()
            .Function().End()
            .Code()
            .End();
        const module = new WebAssembly.Module(builder.WebAssembly().get());
        assert.throws(() => new WebAssembly.Instance(module, {imp: {table}}), WebAssembly.LinkError, "Table import imp:table is not an instance of WebAssembly.Table (evaluating 'new WebAssembly.Instance(module, {imp: {table}})')");
    }
    assertBadTable(25);
    assertBadTable(new Object);
    assertBadTable([]);
    assertBadTable(new WebAssembly.Memory({initial:1}));
}

{
    const builder = new Builder()
        .Type().End()
        .Import()
            .Table("imp", "table", {initial: 25, element: "funcref"})
        .End()
        .Function().End()
        .Export()
            .Table("table", 0)
            .Table("table2", 0)
        .End()
        .Code().End();

    const module = new WebAssembly.Module(builder.WebAssembly().get());
    const table = new WebAssembly.Table({element: "funcref", initial: 25});
    const instance = new WebAssembly.Instance(module, {imp: {table}});
    assert.truthy(table === instance.exports.table);
    assert.truthy(table === instance.exports.table2);
}

{
    const builder = new Builder()
        .Type().End()
        .Function().End()
        .Table()
            .Table({initial: 20, maximum: 30, element: "funcref"})
        .End()
        .Export()
            .Table("table", 0)
            .Table("table2", 0)
        .End()
        .Code().End();

    const module = new WebAssembly.Module(builder.WebAssembly().get());
    const instance = new WebAssembly.Instance(module);
    assert.eq(instance.exports.table, instance.exports.table2);
    assert.eq(instance.exports.table.length, 20);
    assert.truthy(instance.exports.table instanceof WebAssembly.Table);
}

{
    const builder = new Builder()
        .Type().End()
        .Function().End()
        .Table()
            .Table({initial: 0, maximum: 1, element: "funcref"})
            .Table({initial: 20, maximum: 30, element: "funcref"})
        .End()
        .Export()
            .Table("table0", 0)
            .Table("table", 1)
            .Table("table2", 1)
        .End()
        .Code().End();

    const module = new WebAssembly.Module(builder.WebAssembly().get());
    const instance = new WebAssembly.Instance(module);
    assert.eq(instance.exports.table, instance.exports.table2);
    assert.eq(instance.exports.table.length, 20);
    assert.eq(instance.exports.table0.length, 0);
    assert.truthy(instance.exports.table instanceof WebAssembly.Table);
}
