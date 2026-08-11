import Builder from '../Builder.js'
import * as assert from '../assert.js'

const pageSize = 64 * 1024;
const numPages = 1;

var instA;
var instB;
var grow;
var memoryDescription = {initial: numPages};
var mem = new WebAssembly.Memory(memoryDescription);

function test() {
    assert.eq(instA.exports.main(), 42);
}

{
    const b = new Builder();
    b.Type().End()
        .Import()
            .Memory("context", "mem", { initial: 0 })
        .End()
        .Function().End()
        .Exception().Signature({ params: []}).End()
        .Export()
            .Function("throw")
            .Exception("ex", 0)
        .End()
        .Code()
            .Function("throw", { params: [], ret: "void" })
                .Throw(0)
            .End()
        .End()


    const bin = b.WebAssembly().get();
    const module = new WebAssembly.Module(bin);
    instB = new WebAssembly.Instance(module, { context: { mem: new WebAssembly.Memory({ initial: 0 }) } });
}

{
    const b = new Builder();
    b.Type().End()
        .Import()
            .Memory("context", "mem", memoryDescription)
            .Function("context", "throw", { params: [], ret: "void" })
            .Exception("context", "ex", { params: [] })
        .End()
        .Function().End()
        .Exception().Signature({ params: ["i32"]}).End()
        .Export()
            .Function("main")
        .End()
        .Code()
            .Function("main", { params: [], ret: "i32" })
                .I32Const(0)
                .I32Const(42)
                .I32Store(0, 0)
                .Try("i32")
                    .Call(0)
                    .I32Const(0)
                .Catch(0)
                    .I32Const(0)
                    .I32Load(0, 0)
                .End()
            .End()
        .End()


    const bin = b.WebAssembly().get();
    const module = new WebAssembly.Module(bin);
    instA = new WebAssembly.Instance(module, { context: { ...instB.exports, mem } });
}

test();
