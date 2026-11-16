import Builder from '../Builder.js';
import * as assert from '../assert.js';

{
    const m = new WebAssembly.Module(
        (new Builder())
            .Type().End()
            .Import()
                .Function("fooFunction", "barFunction", { params: [] })
                .Table("fooTable", "barTable", { initial: 20, element: "funcref" })
                .Memory("fooMemory", "barMemory", { initial: 20 })
                .Global().I32("fooGlobal", "barGlobal", "immutable").End()
            .End()
            .WebAssembly().get());
    const imports = WebAssembly.Module.imports(m);
    assert.eq(imports.length, 4);
    for (const imp of imports)
        assert.isUndef(imp.type);
}

{
    const m = new WebAssembly.Module(
        (new Builder())
            .Type().End()
            .Function().End()
            .Table()
                .Table({ initial: 20, maximum: 30, element: "funcref" })
            .End()
            .Memory().InitialMaxPages(1, 1).End()
            .Global().I32(42, "immutable").End()
            .Export()
                .Function("func")
                .Table("tab", 0)
                .Memory("mem", 0)
                .Global("glob", 0)
            .End()
            .Code()
                .Function("func", { params: [] }).Return().End()
            .End()
            .WebAssembly().get());
    const exports = WebAssembly.Module.exports(m);
    assert.eq(exports.length, 4);
    for (const exp of exports)
        assert.isUndef(exp.type);
}

{
    // (module (global (export "g") (ref func) (ref.func 0)) (func))
    const bytes = new Uint8Array([0, 97, 115, 109, 1, 0, 0, 0, 1, 4, 1, 96, 0, 0, 3, 2, 1, 0, 6, 7, 1, 100, 112, 0, 210, 0, 11, 7, 5, 1, 1, 103, 3, 0, 10, 4, 1, 2, 0, 11]);
    const module = new WebAssembly.Module(bytes);
    const exports = WebAssembly.Module.exports(module);
    assert.eq(exports.length, 1);
    assert.eq(exports[0].name, "g");
    assert.eq(exports[0].kind, "global");
    assert.isUndef(exports[0].type);
}

{
    // (module (import "m" "g" (global (ref func))))
    const bytes = new Uint8Array([0, 97, 115, 109, 1, 0, 0, 0, 2, 9, 1, 1, 109, 1, 103, 3, 100, 112, 0]);
    const module = new WebAssembly.Module(bytes);
    const imports = WebAssembly.Module.imports(module);
    assert.eq(imports.length, 1);
    assert.eq(imports[0].module, "m");
    assert.eq(imports[0].name, "g");
    assert.eq(imports[0].kind, "global");
    assert.isUndef(imports[0].type);
}
