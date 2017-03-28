import Builder from '../Builder.js';
import * as assert from '../assert.js';

assert.throws(() => WebAssembly.Module.prototype.exports(undefined, ""), TypeError, `WebAssembly.Module.prototype.exports called with non WebAssembly.Module |this| value`);

{
    const m = new WebAssembly.Module((new Builder()).WebAssembly().get());
    assert.isArray(m.exports);
    assert.eq(m.exports.length, 0);
    assert.truthy(m.exports !== m.exports);
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
    assert.eq(m.exports.length, 4);
    assert.eq(m.exports[0].name, "func");
    assert.eq(m.exports[0].kind, "function");
    assert.eq(m.exports[1].name, "tab");
    assert.eq(m.exports[1].kind, "table");
    assert.eq(m.exports[2].name, "mem");
    assert.eq(m.exports[2].kind, "memory");
    assert.eq(m.exports[3].name, "glob");
    assert.eq(m.exports[3].kind, "global");
}
