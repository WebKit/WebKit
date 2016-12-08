import * as assert from '../assert.js';
import Builder from '../Builder.js';

(function EmptyModule() {
    const builder = new Builder();
    const bin = builder.WebAssembly().get();
    const module = new WebAssembly.Module(bin);
    const instance = new WebAssembly.Instance(module);
    assert.instanceof(instance, WebAssembly.Instance);
})();

(function ExportedAnswerI32() {
    const builder = (new Builder())
        .Type().End()
        .Function().End()
        .Export()
            .Function("answer")
        .End()
        .Code()
            .Function("answer", { params: [], ret: "i32" })
                .I32Const(42)
                .Return()
            .End()
        .End();
    const bin = builder.WebAssembly().get();
    const module = new WebAssembly.Module(bin);
    const instance = new WebAssembly.Instance(module);
    const result = instance.exports.answer();
    assert.isA(result, "number");
    assert.eq(result, 42);
})();

/* FIXME this currently crashes as detailed in https://bugs.webkit.org/show_bug.cgi?id=165591
(function Import() {
    let counter = 0;
    const counterSetter = v => counter = v;
    const builder = (new Builder())
        .Type().End()
        .Import()
            .Function("imp", "func", { params: ["i32"] })
        .End()
        .Function().End()
        .Export()
            .Function("changeCounter")
        .End()
        .Code()
            .Function("changeCounter", { params: ["i32"] })
                .I32Const(42)
                .GetLocal(0)
                .I32Add()
                .Call(0) // Calls func(param[0] + 42).
            .End()
        .End();
    const bin = builder.WebAssembly().get();
    const module = new WebAssembly.Module(bin);
    const instance = new WebAssembly.Instance(module, { imp: { func: counterSetter } });
    instance.exports.changeCounter(0);
    assert.eq(counter, 42);
    instance.exports.changeCounter(1);
    assert.eq(counter, 43);
    instance.exports.changeCounter(42);
    assert.eq(counter, 84);
})();
*/
