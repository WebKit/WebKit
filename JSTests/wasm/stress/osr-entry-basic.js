import Builder from '../Builder.js'
import * as assert from '../assert.js'

{
    const b = new Builder();
    b.Type().End()
        .Function().End()
        .Export()
            .Function("loop")
        .End()
        .Code()
        .Function("loop", { params: ["i32"], ret: "i32" }, ["i32"])
        .I32Const(0)
        .SetLocal(1)
        .Loop("void")
        .Block("void", b =>
               b.GetLocal(0)
               .I32Const(0)
               .I32Eq()
               .BrIf(0)
               .GetLocal(0)
               .GetLocal(1)
               .I32Add()
               .SetLocal(1)
               .GetLocal(0)
               .I32Const(1)
               .I32Sub()
               .SetLocal(0)
               .Br(1)
              )
        .End()
        .GetLocal(1)
        .Return()
        .End()
        .End()

    const bin = b.WebAssembly().get();
    const module = new WebAssembly.Module(bin);
    const instance = new WebAssembly.Instance(module);

    assert.eq(987459712, instance.exports.loop(100000000));
}
{
    const b = new Builder();
    b.Type().End()
        .Function().End()
        .Export()
            .Function("loop")
        .End()
        .Code()
        .Function("loop", { params: ["i32", "f32"], ret: "f32" }, ["f32"])
        .F32Const(0)
        .SetLocal(2)
        .Loop("void")
        .Block("void", b =>
               b.GetLocal(0)
               .I32Const(0)
               .I32Eq()
               .BrIf(0)
               .GetLocal(1)
               .GetLocal(2)
               .F32Add()
               .SetLocal(2)
               .GetLocal(0)
               .I32Const(1)
               .I32Sub()
               .SetLocal(0)
               .Br(1)
              )
        .End()
        .GetLocal(2)
        .Return()
        .End()
        .End()

    const bin = b.WebAssembly().get();
    const module = new WebAssembly.Module(bin);
    const instance = new WebAssembly.Instance(module);

    assert.eq(1087937, instance.exports.loop(10000000, 0.1));
}
{
    const b = new Builder();
    b.Type().End()
        .Function().End()
        .Export()
            .Function("loop")
        .End()
        .Code()
        .Function("loop", { params: ["i32", "f64"], ret: "f64" }, ["f64"])
        .F64Const(0)
        .SetLocal(2)
        .Loop("void")
        .Block("void", b =>
               b.GetLocal(0)
               .I32Const(0)
               .I32Eq()
               .BrIf(0)
               .GetLocal(1)
               .GetLocal(2)
               .F64Add()
               .SetLocal(2)
               .GetLocal(0)
               .I32Const(1)
               .I32Sub()
               .SetLocal(0)
               .Br(1)
              )
        .End()
        .GetLocal(2)
        .Return()
        .End()
        .End()

    const bin = b.WebAssembly().get();
    const module = new WebAssembly.Module(bin);
    const instance = new WebAssembly.Instance(module);

    assert.eq(999999.9998389754, instance.exports.loop(10000000, 0.1));
}
{
    const b = new Builder();
    b.Type().End()
        .Function().End()
        .Export()
            .Function("loop")
        .End()
        .Code()
        .Function("loop", { params: ["i32"], ret: "i32" }, ["i64"])
        .I64Const(0)
        .SetLocal(1)
        .Loop("void")
        .Block("void", b =>
               b.GetLocal(0)
               .I32Const(0)
               .I32Eq()
               .BrIf(0)
               .I64Const(3)
               .GetLocal(1)
               .I64Add()
               .SetLocal(1)
               .GetLocal(0)
               .I32Const(1)
               .I32Sub()
               .SetLocal(0)
               .Br(1)
              )
        .End()
        .GetLocal(1)
        .I32WrapI64()
        .Return()
        .End()
        .End()

    const bin = b.WebAssembly().get();
    const module = new WebAssembly.Module(bin);
    const instance = new WebAssembly.Instance(module);

    assert.eq(30000000, instance.exports.loop(10000000));
}
