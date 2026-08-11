import Builder from '../Builder.js'
import * as assert from '../assert.js'

const b = new Builder();
b.Type().End()
    .Function().End()
    .Export()
        .Function("ret5")
    .End()
    .Code()
    .Function("ret5", { params: [], ret: "i32" })
    .I32Const(5)
    .Return()
    .End()
    .End();

const bin = b.WebAssembly().get();
const module = new WebAssembly.Module(bin);
const instance = new WebAssembly.Instance(module);

assert.eq(instance.exports.ret5(), 5);
