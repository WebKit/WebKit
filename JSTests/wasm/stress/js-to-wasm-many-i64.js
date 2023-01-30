import * as assert from '../assert.js';
import Builder from '../Builder.js';

async function test() {
    let params = [];
    for (let i = 0; i < 50; ++i)
        params.push('i64');

    let cont = (new Builder())
        .Type().End()
        .Function().End()
        .Export()
            .Function("foo")
        .End()
        .Code()
        .Function("foo", { params: params, ret: "i64" });
    for (let i = 0; i < 50; ++i)
        cont = cont.GetLocal(i);
    for (let i = 0; i < 49; ++i)
        cont = cont.I64Add();
    let builder = cont.Return()
        .End()
        .End();

    const bin = builder.WebAssembly().get();
    const {instance} = await WebAssembly.instantiate(bin, {});

    for (let i = 0; i < 1000000; i++) {
        assert.eq(instance.exports.foo(
             0n, 1n, 2n, 3n, 4n, 5n, 6n, 7n, 8n, 9n,
            10n,11n,12n,13n,14n,15n,16n,17n,18n,19n,
            20n,21n,22n,23n,24n,25n,26n,27n,28n,29n,
            30n,31n,32n,33n,34n,35n,36n,37n,38n,39n,
            40n,41n,42n,43n,44n,45n,46n,47n,48n,49n,
        ), 1225n);
    }
}

assert.asyncTest(test());
