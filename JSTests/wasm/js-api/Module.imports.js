import Builder from '../Builder.js';
import * as assert from '../assert.js';

assert.throws(() => WebAssembly.Module.prototype.imports(undefined, ""), TypeError, `WebAssembly.Module.prototype.imports called with non WebAssembly.Module |this| value`);

{
    const m = new WebAssembly.Module((new Builder()).WebAssembly().get());
    assert.isArray(m.imports);
    assert.eq(m.imports.length, 0);
    assert.truthy(m.exports !== m.exports);
}

{
    const m = new WebAssembly.Module(
        (new Builder())
            .Type().End()
            .Import()
                .Function("fooFunction", "barFunction", { params: [] })
                .Table("fooTable", "barTable", {initial: 20, element: "anyfunc"})
                .Memory("fooMemory", "barMemory", {initial: 20})
                .Global().I32("fooGlobal", "barGlobal", "immutable").End()
            .End()
            .WebAssembly().get());
    assert.eq(m.imports.length, 4);
    assert.eq(m.imports[0].module, "fooFunction");
    assert.eq(m.imports[0].name, "barFunction");
    assert.eq(m.imports[0].kind, "function");
    assert.eq(m.imports[1].module, "fooTable");
    assert.eq(m.imports[1].name, "barTable");
    assert.eq(m.imports[1].kind, "table");
    assert.eq(m.imports[2].module, "fooMemory");
    assert.eq(m.imports[2].name, "barMemory");
    assert.eq(m.imports[2].kind, "memory");
    assert.eq(m.imports[3].module, "fooGlobal");
    assert.eq(m.imports[3].name, "barGlobal");
    assert.eq(m.imports[3].kind, "global");
}
