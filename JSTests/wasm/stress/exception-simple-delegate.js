import Builder from '../Builder.js'
import * as assert from '../assert.js'

function testSimpleThrowDelegate() {
    const b = new Builder();
    b.Type().End()
        .Function().End()
        .Exception().Signature({ params: []}).End()
        .Export().Function("call")
            .Exception("foo", 0)
        .End()
        .Code()
            .Function("call", { params: [], ret: "i32" })
                .Try("i32")
                    .Try("void")
                        .Throw(0)
                    .Delegate(0)
                .Catch(0)
                    .I32Const(2)
                .End()
            .End()
        .End()


    const bin = b.WebAssembly().get();
    const module = new WebAssembly.Module(bin);
    const instance = new WebAssembly.Instance(module, { });

    assert.eq(instance.exports.call(), 2, "catching an exported wasm tag thrown from JS should be possible");
}

function testThrowDelegateSkip() {
    const b = new Builder();
    b.Type().End()
        .Function().End()
        .Exception().Signature({ params: []}).End()
        .Export().Function("call")
            .Exception("foo", 0)
        .End()
        .Code()
            .Function("call", { params: [], ret: "i32" })
                .Try("i32")
                    .Try("i32")
                        .Try("void")
                            .Throw(0)
                        .Delegate(1)
                    .Catch(0)
                        .I32Const(1)
                    .End()
                .Catch(0)
                    .I32Const(2)
                .End()
            .End()
        .End()


    const bin = b.WebAssembly().get();
    const module = new WebAssembly.Module(bin);
    const instance = new WebAssembly.Instance(module, { });

    assert.eq(instance.exports.call(), 2, "catching an exported wasm tag thrown from JS should be possible");
}

function testDelegateCaller() {
    const b = new Builder();
    b.Type().End()
        .Function().End()
        .Exception().Signature({ params: []}).End()
        .Export().Function("call")
            .Exception("foo", 0)
        .End()
        .Code()
            .Function("call", { params: [], ret: "i32" })
                .Try("i32")
                    .Call(1)
                .Catch(0)
                    .I32Const(2)
                .End()
            .End()

            .Function("call2", { params: [], ret: "i32" })
                .Try("i32")
                    .Try("void")
                        .Throw(0)
                    .Delegate(1)
                .Catch(0)
                    .I32Const(1)
                .End()
            .End()
        .End()


    const bin = b.WebAssembly().get();
    const module = new WebAssembly.Module(bin);
    const instance = new WebAssembly.Instance(module, { });

    assert.eq(instance.exports.call(), 2, "catching an exported wasm tag thrown from JS should be possible");
}

function testSimpleDelegateMerge(){
 const b = new Builder();
    b.Type().End()
        .Function().End()
        .Export()
            .Function("call")
        .End()
        .Code()
            .Function("call", { params: [], ret: "i32" })
                .Try("i32")
                    .I32Const(3)
                 .Delegate(0)
            .End()
        .End()


    const bin = b.WebAssembly().get();
    const module = new WebAssembly.Module(bin);
    const instance = new WebAssembly.Instance(module, { });
    assert.eq(instance.exports.call(), 3);
}

testSimpleThrowDelegate();
testThrowDelegateSkip();
testDelegateCaller();
testSimpleDelegateMerge();
