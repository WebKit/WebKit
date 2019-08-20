import Builder from '../Builder.js'
import * as assert from '../assert.js'

{
    const b = new Builder();
    const locals = [];
    const numLocals = 99;
    for (let i = 0; i < numLocals; ++i)
        locals[i] = "f64";
    let cont = b.Type().End()
        .Function().End()
        .Export()
            .Function("loop")
        .End()
        .Code()
        .Function("loop", { params: ["i32"], ret: "f64" }, locals);
    for (let i = 0; i < numLocals; ++i)
        cont = cont.F64Const(0.00001000000000001).SetLocal(i + 1);
    cont.Loop("void")
        .Block("void", b => {
            let cont = b.GetLocal(0)
                        .I32Const(0)
                        .I32Eq()
                        .BrIf(0);
            for (let i = 0; i < numLocals; ++i)
                cont = cont.GetLocal(i + 1);
            for (let i = 0; i < (numLocals - 1); ++i)
                cont = cont.F64Add();
            cont = cont.SetLocal(1);
            for (let i = 1; i < numLocals; ++i)
                cont = cont.GetLocal(i + 1).F64Const(0.000000000000001).F64Add().SetLocal(i + 1);
            return cont.GetLocal(0)
                .I32Const(1)
                .I32Sub()
                .SetLocal(0)
                .Br(1)
            })
        .End()
        .GetLocal(1)
        .Return()
        .End()
        .End()

    const bin = b.WebAssembly().get();
    const module = new WebAssembly.Module(bin);
    const instance = new WebAssembly.Instance(module);

    assert.eq(980.0485099680602, instance.exports.loop(1000000));
}
