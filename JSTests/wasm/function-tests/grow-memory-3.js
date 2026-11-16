import Builder from '../Builder.js';
import * as assert from '../assert.js';

{
    const memoryDescription = {initial: 0, maximum: 2};
    const builder = (new Builder())
        .Type().End()
        .Import()
            .Memory("imp", "memory", memoryDescription)
        .End()
        .Function().End()
        .Export()
            .Function("foo")
            .Function("bar")
        .End()
        .Code()
            .Function("foo", {params: [], ret: "void"})
                .Unreachable()
                .GrowMemory(0)
            .End()
            .Function("bar", {params: [], ret: "void"})
                .Unreachable()
                .CurrentMemory(0)
            .End()
        .End();

    const bin = builder.WebAssembly().get();
    const module = new WebAssembly.Module(bin); // Just make sure it parses.
    const memory = new WebAssembly.Memory(memoryDescription);
    const instance = new WebAssembly.Instance(module, {imp: {memory}});

    assert.throws(() => instance.exports.foo(), WebAssembly.RuntimeError, "Unreachable code should not be executed")
    assert.throws(() => instance.exports.bar(), WebAssembly.RuntimeError, "Unreachable code should not be executed")
}
