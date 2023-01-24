import * as assert from '../assert.js';
import Builder from '../Builder.js';

async function test() {
    let params = [];
    for (let i = 0; i < 50; ++i)
        params.push('f64');

    let cont = (new Builder())
        .Type().End()
        .Function().End()
        .Export()
            .Function("foo")
        .End()
        .Code()
        .Function("foo", { params: params, ret: "f64" });
    for (let i = 0; i < 50; ++i)
        cont = cont.GetLocal(i);
    for (let i = 0; i < 49; ++i)
        cont = cont.F64Add();
    let builder = cont.Return()
        .End()
        .End();

    const bin = builder.WebAssembly().get();
    const {instance} = await WebAssembly.instantiate(bin, {});

    for (let i = 0; i < 1000000; i++) {
        assert.eq(instance.exports.foo(
            0,1,2,3,4,5,6,7,8,9,
            10,11,12,13,14,15,16,17,18,19,
            20,21,22,23,24,25,26,27,28,29,
            30,31,32,33,34,35,36,37,38,39,
            40,41,42,43,44,45,46,47,48,49,
        ), 1225);
    }
}

assert.asyncTest(test());
