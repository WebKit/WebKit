import * as assert from '../assert.js';
import Builder from '../Builder.js';

let builder = (new Builder())
    .Type().End()
    .Function().End()
    .Export()
        .Function("func")
    .End()
    .Code()
    .Function("func", { params: [] })
        .Return()
    .End()
.End();
const bin = builder.WebAssembly().get();
const module = new WebAssembly.Module(bin);
const instance = new WebAssembly.Instance(module);

assert.isUndef(instance.exports.func());
