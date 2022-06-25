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
                    .I32Const(42)
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

function testSimpleThrowDelegateIllegal() {
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
    assert.throws(() => {
        const module = new WebAssembly.Module(bin);
    }, WebAssembly.CompileError, `WebAssembly.Module doesn't validate:  block with type: () -> [I32] returns: 1 but stack has: 0 values, in function at index 0 (evaluating 'new WebAssembly.Module(bin)')`);
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
                        .I32Const(42)
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

function testThrowDelegateSkipIllegal() {
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
    assert.throws(() => {
        const module = new WebAssembly.Module(bin);
    }, WebAssembly.CompileError, `WebAssembly.Module doesn't validate:  block with type: () -> [I32] returns: 1 but stack has: 0 values, in function at index 0 (evaluating 'new WebAssembly.Module(bin)')`);
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
                    .I32Const(42)
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

function testDelegateCallerIllegal() {
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
    assert.throws(() => {
        const module = new WebAssembly.Module(bin);
    }, WebAssembly.CompileError, `WebAssembly.Module doesn't validate:  block with type: () -> [I32] returns: 1 but stack has: 0 values, in function at index 1 (evaluating 'new WebAssembly.Module(bin)')`);
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
testSimpleThrowDelegateIllegal();
testThrowDelegateSkip();
testThrowDelegateSkipIllegal();
testDelegateCaller();
testDelegateCallerIllegal();
testSimpleDelegateMerge();
