import Builder from '../Builder.js';
import * as assert from '../assert.js';

{
    const memoryDescription = {initial: 0, maximum: 50};
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
            .Function("foo", {params: [], ret: "i32"})
                .Call(0)
                .CurrentMemory(0)
                .Return()
            .End()
        .End();

    const bin = builder.WebAssembly().get();
    const module = new WebAssembly.Module(bin);
    const memory = new WebAssembly.Memory(memoryDescription);

    const func = () => {
        memory.grow(42);
    };

    const instance = new WebAssembly.Instance(module, {imp: {memory, func}});
    assert.eq(instance.exports.foo(), 42);
}
