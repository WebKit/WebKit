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
