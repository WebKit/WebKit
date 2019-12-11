import Builder from '../Builder.js';
import * as assert from '../assert.js';

function escape(){}
noInline(escape);

for (let i = 0; i < 10; ++i) {
    const max = 1024*2;
    const memoryDescription = {initial: 0, maximum: max};
    const growMemoryAmount = 256;

    const builder = (new Builder())
        .Type().End()
        .Import()
            .Function("imp", "func", {params: [], ret: "void"})
            .Memory("imp", "memory", memoryDescription)
        .End()
        .Function().End()
        .Export()
            .Function("foo")
        .End()
        .Code()
            .Function("foo", {params: [], ret: "i32"})
                .I32Const(1)
                .I32Const(1)
                .I32Const(1)
                .I32Const(1)
                .I32Const(1)
                .I32Const(1)
                .I32Const(1)
                .I32Const(1)
                .I32Const(1)
                .I32Const(1)
                .I32Const(1)
                .I32Const(1)
                .I32Const(1)
                .I32Const(1)
                .I32Const(1)
                .I32Const(1)
                .Call(2)
                .I32Const(growMemoryAmount)
                .GrowMemory(0)
                .Return()
            .End()
            .Function("bar", {params: ["i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32"], ret: "void"})
                .Call(0)
                .Return()
            .End()
        .End();

    const bin = builder.WebAssembly().get();
    const module = new WebAssembly.Module(bin);
    const memory = new WebAssembly.Memory(memoryDescription);

    function func() { }

    const instance = new WebAssembly.Instance(module, {imp: {memory, func}});
    for (let i = 0; i < max/growMemoryAmount; ++i) {
        instance.exports.foo();
    }
}
