import Builder from '../Builder.js'
import * as assert from '../assert.js'

{
    const b = new Builder();
    const locals = [];
    for (let i = 0; i < 100; ++i)
        locals[i] = "f64";
    let cont = b.Type().End()
        .Function().End()
        .Export()
            .Function("loop")
        .End()
        .Code()
        .Function("loop", { params: ["i32"], ret: "f64" }, locals);
    for (let i = 0; i < 100; ++i)
        cont = cont.F64Const(i * 0.00000000000000298).SetLocal(i + 1);
    for (let i = 0; i < 100; ++i)
        cont = cont.F64Const(i * 0.00000000000012029810392);
    cont = cont.Loop("f64")
        .Block("f64", b => {
            let cont = b.F64Const(1)
                        .GetLocal(0)
                        .I32Const(0)
                        .I32Eq()
                        .BrIf(0)
                        .Drop();
            for (let i = 0; i < 100; ++i)
                cont = cont.GetLocal(i + 1);
            for (let i = 0; i < 99; ++i)
                cont = cont.F64Add();
            cont = cont.SetLocal(1);
            for (let i = 0; i < 100; ++i)
                cont = cont.GetLocal(i + 1).F64Const(0.000000000012).F64Add().SetLocal(i + 1);
            return cont.GetLocal(0)
                .I32Const(1)
                .I32Sub()
                .SetLocal(0)
                .F64Const(1)
                .Br(1)
                .Drop();
            })
        .End();
    for (let i = 0; i < 100; ++i)
        cont = cont.F64Add();
    cont.GetLocal(1)
        .F64Add()
        .Return()
        .End()
        .End();

    const bin = b.WebAssembly().get();
    const module = new WebAssembly.Module(bin);
    const instance = new WebAssembly.Instance(module);

    assert.eq(594.9994327521441, instance.exports.loop(1000000));
}
