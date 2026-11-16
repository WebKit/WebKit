import * as assert from '../assert.js';
import Builder from '../Builder.js';

const builder = (new Builder())

builder.Type().End()
    .Function().End()
    .Memory().InitialMaxPages(1, 1).End()
    .Export()
        .Function("callFunc")
    .End()
    .Code()
        .Function("callFunc", { params: ["i32", "i32"], ret: "i32" })
            .GetLocal(0)
            .If("void", b => {
                return b.GetLocal(0)
                .GetLocal(1)
                .I32Load(0, 4)
                .I32Sub()
                .Return()
            })
            .I32Const(42)
            .Return()
        .End()
    .End();
const bin = builder.WebAssembly().get();
const module = new WebAssembly.Module(bin);
const instance = new WebAssembly.Instance(module);
