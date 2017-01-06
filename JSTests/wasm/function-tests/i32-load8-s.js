import Builder from '../Builder.js'
import * as assert from '../assert.js'

const b = new Builder();
b.Type().End()
    .Function().End()
    .Memory().InitialMaxPages(1, 1).End()
    .Export().Function("foo").End()
    .Code()
        .Function("foo", { params: ["i32", "i32"], ret: "i32" })
            .GetLocal(1)
            .GetLocal(0)
            .I32Store(2, 0)
            .GetLocal(1)
            .I32Load8S(2, 0)
            .Return()
        .End()
    .End()

const bin = b.WebAssembly().get();
const foo = (new WebAssembly.Instance(new WebAssembly.Module(bin))).exports.foo;
assert.eq(foo(0, 10), 0);
assert.eq(foo(100, 112), 100);
assert.eq(foo(1000000, 10), 0x40);
