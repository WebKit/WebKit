import Builder from '../Builder.js'
import * as assert from '../assert.js'

{
    const b = new Builder();
    const locals = [];
    for (let i = 0; i < 100; ++i)
        locals[i] = "i32";
    let cont = b.Type().End()
        .Function().End()
        .Export()
            .Function("loop")
        .End()
        .Code()
        .Function("loop", { params: ["i32"], ret: "i32" }, locals);
    for (let i = 0; i < 100; ++i)
        cont = cont.I32Const(i).SetLocal(i + 1);
    for (let i = 0; i < 100; ++i)
        cont = cont.I32Const(i);
    cont = cont.Loop("i32")
        .Block("i32", b => {
            let cont = b.I32Const(1)
                        .GetLocal(0)
                        .I32Const(0)
                        .I32Eq()
                        .BrIf(0)
                        .Drop();
            for (let i = 0; i < 100; ++i)
                cont = cont.GetLocal(i + 1);
            for (let i = 0; i < 99; ++i)
                cont = cont.I32Add();
            cont = cont.SetLocal(1);
            for (let i = 0; i < 100; ++i)
                cont = cont.GetLocal(i + 1).I32Const(1).I32Add().SetLocal(i + 1);
            return cont.GetLocal(0)
                .I32Const(1)
                .I32Sub()
                .SetLocal(0)
                .I32Const(1)
                .Br(1)
                .Drop();
            })
        .End();
    for (let i = 0; i < 100; ++i)
        cont = cont.I32Add();
    cont.GetLocal(1)
        .I32Add()
        .Return()
        .End()
        .End();

    const bin = b.WebAssembly().get();
    const module = new WebAssembly.Module(bin);
    const instance = new WebAssembly.Instance(module);

    assert.eq(2041858167, instance.exports.loop(3000000));
}
