import Builder from '../Builder.js'
import * as assert from '../assert.js'

{
    const b = new Builder();
    b.Type().End()
        .Import().Function("context", "callback", { params: [], ret: "void" }).End()
        .Function().End()
        .Exception().Signature({ params: []}).End()
        .Export().Function("call")
            .Exception("foo", 0)
        .End()
        .Code()
            .Function("call", { params: [], ret: "i32" })
            .Try("i32")
                .Call(0)
                .I32Const(1)
            .CatchAll()
                .I32Const(2)
                .Rethrow(0)
            .End()
            .End()
        .End()


    const bin = b.WebAssembly().get();
    const module = new WebAssembly.Module(bin);
    const instance = new WebAssembly.Instance(module, { context: { callback }});

    let tag = instance.exports.foo;

    function callback() {
        throw new WebAssembly.Exception(tag, []);
    }

    assert.throws(instance.exports.call, WebAssembly.Exception);

    tag = new WebAssembly.Tag({ parameters: [] });

    assert.throws(instance.exports.call, WebAssembly.Exception);
}
