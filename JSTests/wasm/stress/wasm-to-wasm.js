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
        .Function("foo", { params: ["i32"], ret: "i32" }, ["i32"])
        .Block("i32")
        .Loop("i32", b =>
                b.GetLocal(1)
                .GetLocal(0)
                .I32Eqz()
                .BrIf(1)

                .Call(1)
                .GetLocal(1)
                .I32Add()
                .SetLocal(1)
                .GetLocal(0)
                .I32Const(1)
                .I32Sub()
                .SetLocal(0)
                .Br(0)

        )
        .End()
        .End()
        .Function("bar", { params: [], ret: "i32" })
            .I32Const(42)
            .Return()
        .End()
        .End()

    const bin = builder.WebAssembly().get();
    const {instance} = await WebAssembly.instantiate(bin, {});

    const iters = 100000
    for (let i = 0; i < 100; i++)
        assert.eq(instance.exports.foo(iters), 42 * iters);
}

assert.asyncTest(test());
