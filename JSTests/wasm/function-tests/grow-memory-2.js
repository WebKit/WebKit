import Builder from '../Builder.js';
import * as assert from '../assert.js';

{
    const memoryDescription = {initial: 0, maximum: 2};
    const builder = (new Builder())
        .Type().End()
        .Import()
            .Memory("imp", "memory", memoryDescription)
            .Function("imp", "func", {params: [], ret: "void"})
        .End()
        .Function().End()
        .Export()
            .Function("foo")
        .End()
        .Code()
            .Function("foo", {params: ["i32"], ret: "i32"})
                .Call(0)
                .GetLocal(0)
                .I32Load(0, 0)
                .Return()
            .End()
        .End();

    const bin = builder.WebAssembly().get();
    const module = new WebAssembly.Module(bin);
    const memory = new WebAssembly.Memory(memoryDescription);

    const func = () => {
        memory.grow(1);
        (new Uint32Array(memory.buffer))[0] = 42;
    };

    const instance = new WebAssembly.Instance(module, {imp: {memory, func}});
    assert.eq(instance.exports.foo(0), 42);
}

{
    const memoryDescription = {initial: 0, maximum: 2};
    const tableDescription = {initial: 1, maximum: 1, element: "anyfunc"};
    const builder = (new Builder())
        .Type()
            .Func([], "void")
        .End()
        .Import()
            .Memory("imp", "memory", memoryDescription)
            .Function("imp", "func", {params: [], ret: "void"})
            .Table("imp", "table", tableDescription)
        .End()
        .Function().End()
        .Export()
            .Function("foo")
            .Function("bar")
        .End()
        .Code()
            .Function("foo", {params: ["i32"], ret: "i32"})
                .I32Const(0)
                .CallIndirect(0, 0) // call [] => void
                .GetLocal(0)
                .I32Load(0, 0)
                .Return()
            .End()
            .Function("bar", {params: [], ret: "void"})
                .Call(0)
                .Return()
            .End()
        .End();

    const bin = builder.WebAssembly().get();
    const module = new WebAssembly.Module(bin);
    const memory = new WebAssembly.Memory(memoryDescription);
    const table = new WebAssembly.Table(tableDescription);

    const func = () => {
        memory.grow(1);
        (new Uint32Array(memory.buffer))[0] = 0xbadbeef;
    };

    const instance = new WebAssembly.Instance(module, {imp: {memory, func, table}});
    table.set(0, instance.exports.bar);
    assert.eq(instance.exports.foo(0), 0xbadbeef);
}
