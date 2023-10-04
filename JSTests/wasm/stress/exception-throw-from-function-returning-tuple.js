import Builder from '../Builder.js'
import * as assert from '../assert.js'

function testCatchWithExceptionThrownFromFunctionReturningTuple() {
    const b = new Builder();
    b.Type().End()
        .Function().End()
        .Exception().Signature({ params: []}).End()
        .Export().Function("call")
            .Exception("foo", 0)
        .End()
        .Code()

            .Function("call", { params: ["i32", "i32"], ret: "i32" })
                .GetLocal(0)
                .GetLocal(1)
                .Try("i32")
                    .Call(1)
                    .Drop()
                    .Drop()
                .Catch(0)
                    .I32Const(2)
                .End()
                .Drop()
                .I32Add()
            .End()

            .Function("call2", { params: [], ret: ["i32", "i32", "i32"] })
                .Throw(0)
            .End()
        .End();

    const bin = b.WebAssembly().get();
    const module = new WebAssembly.Module(bin);
    const instance = new WebAssembly.Instance(module, { });

    for (let i = 0; i < 1000; ++i)
        assert.eq(instance.exports.call(42, 5), 47, "catching an exported wasm tag thrown from JS should be possible");
}

function testCatchWithExceptionThrownFromFunctionReturningTuple2() {
    const b = new Builder();
    b.Type().End()
        .Function().End()
        .Exception().Signature({ params: []}).End()
        .Export().Function("call")
            .Exception("foo", 0)
        .End()
        .Code()

            .Function("call", { params: ["i32", "i32"], ret: "i32" })
                .GetLocal(0)
                .GetLocal(1)
                .Try("f32")
                    .I32Const(10)
                    .I32Const(10)
                    .Call(1)
                    .Drop()
                    .Drop()
                    .Drop()
                    .Drop()
                .Catch(0)
                    .F32Const(2)
                .End()
                .Drop()
                .I32Add()
            .End()

            .Function("call2", { params: ["i32", "i32"], ret: ["f32", "f32", "f32", "f32", "f32"] })
                .Throw(0)
            .End()
        .End();

    const bin = b.WebAssembly().get();
    const module = new WebAssembly.Module(bin);
    const instance = new WebAssembly.Instance(module, { });

    for (let i = 0; i < 1000; ++i)
        assert.eq(instance.exports.call(42, 5), 47, "catching an exported wasm tag thrown from JS should be possible");
}

function testCatchWithExceptionThrownFromFunctionReturningTuple3() {
    const b = new Builder();
    b.Type().End()
        .Function().End()
        .Exception().Signature({ params: ["i32", "i32"]}).End()
        .Export().Function("call")
            .Exception("foo", 0)
        .End()
        .Code()

            .Function("call", { params: ["i32", "i32"], ret: "i32" })
                .Try("i32")
                    .GetLocal(0)
                    .GetLocal(1)
                    .Call(1)
                    .Drop()
                    .Drop()
                .Catch(0)
                    .I32DivU()
                .End()
            .End()

            .Function("call2", { params: ["i32", "i32"], ret: ["i32", "i32", "i32"] })
                .GetLocal(0)
                .GetLocal(1)
                .Throw(0)
            .End()
        .End();

    const bin = b.WebAssembly().get();
    const module = new WebAssembly.Module(bin);
    const instance = new WebAssembly.Instance(module, { });

    for (let i = 0; i < 1000; ++i)
        assert.eq(instance.exports.call(0, 42), 0, "catching an exported wasm tag thrown from JS should be possible");
}

function testCatchWithExceptionThrownFromJSReturningTuple() {
    const b = new Builder();
    b.Type().End()
        .Import().Function("context", "callback", { params: ["i32", "i32"], ret: ["i32", "i32", "i32"] }).End()
        .Function().End()
        .Exception().Signature({ params: ["i32", "i32"]}).End()
        .Export().Function("call")
            .Exception("foo", 0)
        .End()
        .Code()

            .Function("call", { params: ["i32", "i32"], ret: "i32" })
                .Try("i32")
                    .GetLocal(0)
                    .GetLocal(1)
                    .Call(0)
                    .Drop()
                    .Drop()
                .Catch(0)
                    .I32DivU()
                .End()
            .End()
        .End();

    const bin = b.WebAssembly().get();
    const module = new WebAssembly.Module(bin);
    const instance = new WebAssembly.Instance(module, { context: { callback }});

    let tag = instance.exports.foo;

    function callback(a, b) {
        assert.eq(a, 0);
        assert.eq(b, 42);
        throw new WebAssembly.Exception(tag, [a, b]);
    }

    for (let i = 0; i < 1000; ++i)
        assert.eq(instance.exports.call(0, 42), 0, "catching an exported wasm tag thrown from JS should be possible");
}

testCatchWithExceptionThrownFromFunctionReturningTuple();
testCatchWithExceptionThrownFromFunctionReturningTuple2();
testCatchWithExceptionThrownFromFunctionReturningTuple3();
testCatchWithExceptionThrownFromJSReturningTuple()
