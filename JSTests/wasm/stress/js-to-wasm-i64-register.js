import * as assert from '../assert.js';
import Builder from '../Builder.js';

async function test() {
    let params = [];
    for (let i = 0; i < 1; ++i)
        params.push('i64');

    let cont = (new Builder())
        .Type().End()
        .Function().End()
        .Export()
            .Function("foo")
        .End()
        .Code()
        .Function("foo", { params: params, ret: "i64" });
    for (let i = 0; i < 1; ++i)
        cont = cont.GetLocal(i);
    let builder = cont.Return()
        .End()
        .End();

    const bin = builder.WebAssembly().get();
    const {instance} = await WebAssembly.instantiate(bin, {});

    for (let i = 0; i < 100000; i++) {
        assert.eq(instance.exports.foo(0n), 0n);
        assert.eq(instance.exports.foo(-1n), -1n);
        assert.eq(instance.exports.foo(0xffffffffn), 0xffffffffn);
        assert.eq(instance.exports.foo(0xffffffffffffffffn), -1n);
        assert.eq(instance.exports.foo(0xffffffffffffffffffffn), -1n);
        assert.eq(instance.exports.foo(-0xffffffffn), -0xffffffffn);
        assert.eq(instance.exports.foo(-0xffffffffffffffffn), 1n);
        assert.eq(instance.exports.foo(-0xffffffffffffffffffffn), 1n);
        assert.eq(instance.exports.foo(0x80000000n), 0x80000000n);
        assert.eq(instance.exports.foo(-0x80000000n), -0x80000000n);
        assert.eq(instance.exports.foo(0x8000000000000000n), -9223372036854775808n);
        assert.eq(instance.exports.foo(-0x8000000000000000n), -0x8000000000000000n);
    }
}

assert.asyncTest(test());
