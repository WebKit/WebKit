import Builder from '../Builder.js'
import * as assert from '../assert.js'

const argumentCount = 8;
const counter = argumentCount;

let f = new Builder()
    .Type().End()
    .Function().End()
    .Export().Function("readArgumentsLate").End()
    .Code()
    .Function("readArgumentsLate", { params: new Array(argumentCount).fill("i32"), ret: "i32" }, ["i32"])
        .I32Const(0)
        .SetLocal(counter)
        .Loop("void")
        .Block("void", b => b
            .GetLocal(counter).I32Const(64).I32Eq().BrIf(0)
            .GetLocal(counter).I32Const(1).I32Add().SetLocal(counter)
            .Br(1))
        .End();

f = f.GetLocal(0);
for (let i = 1; i < argumentCount; ++i)
    f = f.GetLocal(i).I32Const(i + 1).I32Mul().I32Add();

const bin = f.Return().End().End().WebAssembly().get();
const instance = new WebAssembly.Instance(new WebAssembly.Module(bin));

const args = [11, 22, 33, 44, 55, 66, 77, 88];
let expected = args[0];
for (let i = 1; i < argumentCount; ++i)
    expected += args[i] * (i + 1);

for (let i = 0; i < 1000; ++i)
    assert.eq(expected, instance.exports.readArgumentsLate(...args));
