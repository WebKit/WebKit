import * as assert from '../assert.js'
import Builder from '../Builder.js'

const b = new Builder();
b.Type().End()
    .Import().End()
    .Function().End()
    .Export()
        .Function("fac")
    .End()
    .Code()
        .Function("fac", { params: ["i32"], ret: "i32" })
            .GetLocal(0)
            .I32Const(0)
            .I32Eq()
            .If("void", b =>
                b.I32Const(1)
                .Return()
               )
                .GetLocal(0)
            .GetLocal(0)
            .I32Const(1)
            .I32Sub()
            .Call(0)
            .I32Mul()
            .Return()
        .End()
    .End()

const m = new WebAssembly.Module(b.WebAssembly().get());
const fac = (new WebAssembly.Instance(m)).exports.fac;
assert.eq(fac(0), 1);
assert.eq(fac(1), 1);
assert.eq(fac(2), 2);
assert.eq(fac(4), 24);
assert.throws(() => fac(1e7), RangeError, "Maximum call stack size exceeded.");
