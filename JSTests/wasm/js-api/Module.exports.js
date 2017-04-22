import Builder from '../Builder.js';
import * as assert from '../assert.js';

assert.throws(() => WebAssembly.Module.exports(undefined), TypeError, `WebAssembly.Module.exports called with non WebAssembly.Module argument`);
assert.eq(WebAssembly.Module.exports.length, 1);

{
    const m = new WebAssembly.Module((new Builder()).WebAssembly().get());
    assert.isArray(WebAssembly.Module.exports(m));
    assert.eq(WebAssembly.Module.exports(m).length, 0);
    assert.truthy(WebAssembly.Module.exports(m) !== WebAssembly.Module.exports(m));
}

{
    const m = new WebAssembly.Module(
        (new Builder())
            .Type().End()
            .Function().End()
            .Table()
                .Table({initial: 20, maximum: 30, element: "anyfunc"})
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
    assert.eq(WebAssembly.Module.exports(m).length, 4);
    assert.eq(WebAssembly.Module.exports(m)[0].name, "func");
    assert.eq(WebAssembly.Module.exports(m)[0].kind, "function");
    assert.eq(WebAssembly.Module.exports(m)[1].name, "tab");
    assert.eq(WebAssembly.Module.exports(m)[1].kind, "table");
    assert.eq(WebAssembly.Module.exports(m)[2].name, "mem");
    assert.eq(WebAssembly.Module.exports(m)[2].kind, "memory");
    assert.eq(WebAssembly.Module.exports(m)[3].name, "glob");
    assert.eq(WebAssembly.Module.exports(m)[3].kind, "global");
}
