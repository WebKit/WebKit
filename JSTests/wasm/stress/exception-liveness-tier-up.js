import Builder from '../Builder.js'
import * as assert from '../assert.js'

{
    const b = new Builder();
    b.Type().End()
        .Function().End()
        .Exception()
            .Signature({ params: ["i32"]})
        .End()
        .Export()
            .Function("call")
            .Exception("tag", 0)
        .End()
        .Code()
            .Function("call", { params: [], ret: "i32" }, ["i32"])
                .I32Const(1e6)
                .SetLocal(0)
                .Try("i32")
                    .I32Const(42)
                    .Throw(0)
                .Catch(0)
                    .Loop("void")
                        .Block("void", b =>
                               b.GetLocal(0)
                               .I32Eqz()
                               .BrIf(0)
                               .GetLocal(0)
                               .I32Const(1)
                               .I32Sub()
                               .SetLocal(0)
                               .Br(1)
                              )
                    .End()
                    .Rethrow(0)
                .End()
            .End()
        .End()


    const bin = b.WebAssembly().get();
    const module = new WebAssembly.Module(bin);
    const instance = new WebAssembly.Instance(module);
    const tag = instance.exports.tag;

    assert.throws(instance.exports.call, WebAssembly.Exception, "wasm exception");
    try {
        instance.exports.call();
    } catch (e) {
        assert.truthy(e.is(tag));
        assert.eq(e.getArg(tag, 0), 42);
    }
}

{
    const b = new Builder();
    b.Type().End()
        .Import().Function("context", "callback", { params: [], ret: "i32" }).End()
        .Function().End()
        .Exception().Signature({ params: []}).End()
        .Export().Function("call")
        .End()
        .Code()
            .Function("call", { params: ["i32"], ret: "i32" })
                .GetLocal(0)
                .Loop("void")
                    .Call(2)
                    .Try("void")
                        .Throw(0)
                    .CatchAll()
                        .Call(0)
                        .I32Eqz()
                        .BrIf(1)
                    .End()
                .End()
            .End()
            .Function("wrapper", { params: [], ret: "void" })
            .End()
        .End()

    var counter = 1e5;
    const bin = b.WebAssembly().get();
    const module = new WebAssembly.Module(bin);
    const instance = new WebAssembly.Instance(module, { context: { callback: function() { if (!--counter) return true;} } });

    assert.eq(instance.exports.call(), 0);
}
