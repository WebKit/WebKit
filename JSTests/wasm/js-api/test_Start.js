import * as assert from '../assert.js';
import Builder from '../Builder.js';

(function StartNamedFunction() {
    const b = (new Builder())
        .Type().End()
        .Import()
            .Function("imp", "func", { params: ["i32"] })
        .End()
        .Function().End()
        .Start("foo").End()
        .Code()
            .Function("foo", { params: [] })
                .I32Const(42)
                .Call(0) // Calls func(42).
            .End()
        .End();
    const bin = b.WebAssembly().get();
    const module = new WebAssembly.Module(bin);
    let value = 0;
    const setter = v => value = v;
    const instance = new WebAssembly.Instance(module, { imp: { func: setter } });
    assert.eq(value, 42);
})();

(function InvalidStartFunctionIndex() {
    const b = (new Builder())
        .setChecked(false)
        .Type().End()
        .Function().End()
        .Start(0).End() // Invalid index.
        .Code().End();
    const bin = b.WebAssembly().get();
    assert.throws(() => new WebAssembly.Module(bin), Error, `couldn't parse section Start: Start function declaration (evaluating 'new WebAssembly.Module(bin)')`);
})();
