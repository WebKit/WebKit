import Builder from '../Builder.js';
import * as assert from '../assert.js';

assert.throws(() => WebAssembly.Module.imports(undefined), TypeError, `WebAssembly.Module.imports called with non WebAssembly.Module argument`);
assert.eq(WebAssembly.Module.imports.length, 1);

{
    const m = new WebAssembly.Module((new Builder()).WebAssembly().get());
    assert.isArray(WebAssembly.Module.imports(m));
    assert.eq(WebAssembly.Module.imports(m).length, 0);
    assert.truthy(WebAssembly.Module.imports(m) !== WebAssembly.Module.imports(m));
}

{
    const m = new WebAssembly.Module(
        (new Builder())
            .Type().End()
            .Import()
                .Function("fooFunction", "barFunction", { params: [] })
                .Table("fooTable", "barTable", {initial: 20, element: "funcref"})
                .Memory("fooMemory", "barMemory", {initial: 20})
                .Global().I32("fooGlobal", "barGlobal", "immutable").End()
            .End()
            .WebAssembly().get());
    assert.eq(WebAssembly.Module.imports(m).length, 4);
    assert.eq(WebAssembly.Module.imports(m)[0].module, "fooFunction");
    assert.eq(WebAssembly.Module.imports(m)[0].name, "barFunction");
    assert.eq(WebAssembly.Module.imports(m)[0].kind, "function");
    assert.eq(WebAssembly.Module.imports(m)[0].type.parameters, []);
    assert.eq(WebAssembly.Module.imports(m)[0].type.results, []);
    assert.eq(WebAssembly.Module.imports(m)[1].module, "fooTable");
    assert.eq(WebAssembly.Module.imports(m)[1].name, "barTable");
    assert.eq(WebAssembly.Module.imports(m)[1].kind, "table");
    assert.eq(WebAssembly.Module.imports(m)[1].type.minimum, 20);
    assert.eq(WebAssembly.Module.imports(m)[1].type.maximum, undefined);
    assert.eq(WebAssembly.Module.imports(m)[1].type.element, "funcref");
    assert.eq(WebAssembly.Module.imports(m)[2].module, "fooMemory");
    assert.eq(WebAssembly.Module.imports(m)[2].name, "barMemory");
    assert.eq(WebAssembly.Module.imports(m)[2].kind, "memory");
    assert.eq(WebAssembly.Module.imports(m)[2].type.minimum, 20);
    assert.eq(WebAssembly.Module.imports(m)[2].type.maximum, undefined);
    assert.eq(WebAssembly.Module.imports(m)[2].type.shared, false);
    assert.eq(WebAssembly.Module.imports(m)[3].module, "fooGlobal");
    assert.eq(WebAssembly.Module.imports(m)[3].name, "barGlobal");
    assert.eq(WebAssembly.Module.imports(m)[3].kind, "global");
    assert.eq(WebAssembly.Module.imports(m)[3].type.value, "i32");
    assert.eq(WebAssembly.Module.imports(m)[3].type.mutable, false);
}
