import Builder from '../Builder.js'
import * as assert from '../assert.js'

var instA;
var instB;

function test() {
    assert.throws(instA.exports.main, WebAssembly.Exception, "wasm exception");
}

{
    const b = new Builder();
    b.Type().End()
        .Function().End()
        .Exception().Signature({ params: []}).End()
        .Export()
            .Function("throw")
        .End()
        .Code()
            .Function("throw", { params: [], ret: "i32" })
                .Throw(0)
            .End()
        .End()


    const bin = b.WebAssembly().get();
    const module = new WebAssembly.Module(bin);
    instB = new WebAssembly.Instance(module);
}

{
    const b = new Builder();
    b.Type().End()
        .Import()
            .Function("context", "throw", { params: [], ret: "i32" })
        .End()
        .Function().End()
        .Exception().Signature({ params: ["i32", "i32"]}).End()
        .Export()
            .Function("main")
        .End()
        .Code()
            .Function("main", { params: [], ret: "i32" })
                .Try("i32")
                    .Call(0)
                .Catch(0)
                    .I32Add()
                .End()
            .End()
        .End()


    const bin = b.WebAssembly().get();
    const module = new WebAssembly.Module(bin);
    instA = new WebAssembly.Instance(module, { context: { throw: instB.exports.throw } });
}

test();
