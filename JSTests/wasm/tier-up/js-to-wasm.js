import * as assert from '../assert.js';
import Builder from '../Builder.js';

async function test() {
    let builder = (new Builder())
        .Type().End()
        .Function().End()
        .Export()
            .Function("foo")
        .End()
        .Code()
        .Function("foo", { params: [], ret: "i32" })
            .I32Const(42)
            .Return()
        .End()
        .End();

    const bin = builder.WebAssembly().get();
    const {instance} = await WebAssembly.instantiate(bin, {});

    for (let i = 0; i < 1000000; i++)
        assert.eq(instance.exports.foo(), 42);
}

assert.asyncTest(test());
