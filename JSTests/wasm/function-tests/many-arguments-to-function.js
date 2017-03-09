import Builder from '../Builder.js'
import * as assert from '../assert.js'

{
    const b = new Builder()
        .Type().End()
        .Import().Function("imp", "func", { params: ["i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32"], ret:"i32" }).End()
        .Function().End()
        .Export().Function("f0").End()
        .Code()
            .Function("f0", { params: [], ret: "i32" })
                .I32Const(0)
                .I32Const(1)
                .I32Const(2)
                .I32Const(3)
                .I32Const(4)
                .I32Const(5)
                .I32Const(6)
                .I32Const(7)
                .I32Const(8)
                .I32Const(9)
                .I32Const(10)
                .I32Const(11)
                .I32Const(12)
                .I32Const(13)
                .I32Const(14)
                .I32Const(15)
                .I32Const(16)
                .I32Const(17)
                .Call(0)
                .Return()
            .End()
        .End()

    function foo(...args) {
        for (let i = 0; i < args.length; i++) {
            if (args[i] !== i)
                throw new Error("Bad!");
        }
    }

    let imp = {imp: {func: foo}}
    let instance = new WebAssembly.Instance(new WebAssembly.Module(b.WebAssembly().get()), imp);
    for (let i = 0; i < 100; i++)
        instance.exports.f0();
}

{
    const b = new Builder()
        .Type().End()
        .Import().Function("imp", "func", { params: ["f32", "f32", "f32", "f32", "f32", "f32", "f32", "f32", "f32", "f32", "f32", "f32", "f32", "f32", "f32", "f32", "f32", "f32"], ret:"f32" }).End()
        .Function().End()
        .Export().Function("f0").End()
        .Code()
            .Function("f0", { params: [], ret: "f32" })
                .F32Const(0.5)
                .F32Const(1.5)
                .F32Const(2.5)
                .F32Const(3.5)
                .F32Const(4.5)
                .F32Const(5.5)
                .F32Const(6.5)
                .F32Const(7.5)
                .F32Const(8.5)
                .F32Const(9.5)
                .F32Const(10.5)
                .F32Const(11.5)
                .F32Const(12.5)
                .F32Const(13.5)
                .F32Const(14.5)
                .F32Const(15.5)
                .F32Const(16.5)
                .F32Const(17.5)
                .Call(0)
                .Return()
            .End()
        .End()

    function foo(...args) {
        for (let i = 0; i < args.length; i++) {
            if (args[i] !== (i + 0.5))
                throw new Error("Bad!");
        }
    }

    let imp = {imp: {func: foo}}
    let instance = new WebAssembly.Instance(new WebAssembly.Module(b.WebAssembly().get()), imp);
    for (let i = 0; i < 100; i++)
        instance.exports.f0();
}

{
    const b = new Builder()
        .Type().End()
        .Import().Function("imp", "func", { params: ["f32", "f32", "f32", "f32", "f32", "f32", "f32", "f32", "f32", "f32", "f32", "f32", "f32", "f32", "f32", "f32", "f32", "f32"], ret:"f32" }).End()
        .Function().End()
        .Export().Function("f0").End()
        .Code()
            .Function("f0", { params: ["f32", "f32", "f32", "f32", "f32", "f32", "f32", "f32", "f32", "f32", "f32", "f32", "f32", "f32", "f32", "f32", "f32", "f32"] , ret: "f32" })
                .GetLocal(0)
                .GetLocal(1)
                .GetLocal(2)
                .GetLocal(3)
                .GetLocal(4)
                .GetLocal(5)
                .GetLocal(6)
                .GetLocal(7)
                .GetLocal(8)
                .GetLocal(9)
                .GetLocal(10)
                .GetLocal(11)
                .GetLocal(12)
                .GetLocal(13)
                .GetLocal(14)
                .GetLocal(15)
                .GetLocal(16)
                .GetLocal(17)
                .Call(0)
                .Return()
            .End()
        .End()

    function foo(...args) {
        for (let i = 0; i < args.length; i++) {
            if (args[i] !== i)
                throw new Error("Bad!");
        }
    }

    let imp = {imp: {func: foo}}
    let instance = new WebAssembly.Instance(new WebAssembly.Module(b.WebAssembly().get()), imp);
    let arr = [];
    for (let i = 0; i < 18; i++)
        arr.push(i);
    for (let i = 0; i < 100; i++)
        instance.exports.f0(...arr);
}


{
    let signature = [];
    function addType(t, i) {
        for (let j = 0; j < i; j++) {
            signature.push(t);
        }
    }
    addType("i32", 16);
    addType("f32", 16);

    let b = new Builder()
        .Type().End()
        .Import().Function("imp", "func", { params: signature, ret:"f32" }).End()
        .Function().End()
        .Export().Function("f0").End()
        .Code()
            .Function("f0", { params: signature , ret: "f32" });
    for (let i = 0; i < (16 + 16); i++) {
        b = b.GetLocal(i);
    }

    b = b.Call(0).Return().End().End();

    function foo(...args) {
        if (args.length !== 32)
            throw new Error("Bad!")

        for (let i = 0; i < 16; i++) {
            if (args[i] !== i)
                throw new Error("Bad!");
            if (args[i + 16] !== (i + 16 + 0.5))
                throw new Error("Bad!");
        }
    }

    let imp = {imp: {func: foo}}
    let instance = new WebAssembly.Instance(new WebAssembly.Module(b.WebAssembly().get()), imp);
    let arr = [];
    for (let i = 0; i < 16; i++)
        arr.push(i);
    for (let i = 16; i < 32; i++)
        arr.push(i + 0.5);
    for (let i = 0; i < 100; i++)
        instance.exports.f0(...arr);
}


