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

    assert.eq(instance.exports.call(), 2, "catch_alling an exported wasm tag thrown from JS should be possible");

    tag = new WebAssembly.Tag({ parameters: [] });

    assert.eq(instance.exports.call(), 2, "catch_alling an opaque wasm tag thrown from JS should be possible");
}
