import Builder from '../Builder.js'
import * as assert from '../assert.js'

function loopOSRWithRethrow() {
    const b = new Builder();
    b.Type().End()
        .Import().Function("context", "callback", { params: [], ret: "void" }).End()
        .Function().End()
        .Exception().Signature({ params: []}).End()
        .Export().Function("call")
            .Exception("foo", 0)
        .End()
        .Code()
            .Function("call", { params: ["i32"], ret: "i32" })
            .Try("i32")
                .Call(0)
                .Unreachable()
            .Catch(0)
                .Loop("i32")

                .GetLocal(0)
                .I32Const(1)
                .I32Add()
                .TeeLocal(0)

                .I32Const(1000)
                .I32LeU()

                .BrIf(0)
                .Rethrow(1)
                .End()
            .End()
            .End()
        .End()


    const bin = b.WebAssembly().get();
    const module = new WebAssembly.Module(bin);
    const instance = new WebAssembly.Instance(module, { context: { callback }});

    let tag = instance.exports.foo;
    let exception;

    function callback() {
        exception = new WebAssembly.Exception(tag, []);
        throw exception;
    }

    try {
        instance.exports.call();
    } catch (e) {
        assert.eq(exception, e);
    }

}

function loopOSRWithMultiRethrow1() {
    const b = new Builder();
    b.Type().End()
        .Import().Function("context", "callback", { params: [], ret: "void" }).End()
        .Function().End()
        .Exception().Signature({ params: []}).End()
        .Export().Function("call")
            .Exception("foo", 0)
        .End()
        .Code()
            .Function("call", { params: ["i32"], ret: "i32" })
            .Try("i32")
                .Call(0)
                .Unreachable()
            .Catch(0)
                .Try("i32")
                .Call(0)
                .Unreachable()
                .CatchAll()
                    .Loop("i32")

                    .GetLocal(0)
                    .I32Const(1)
                    .I32Add()
                    .TeeLocal(0)

                    .I32Const(1000)
                    .I32LeU()

                    .BrIf(0)
                    .Rethrow(1)
                    .End()
                .End()
            .End()
            .End()
        .End()


    const bin = b.WebAssembly().get();
    const module = new WebAssembly.Module(bin);
    const instance = new WebAssembly.Instance(module, { context: { callback }});

    let tag = instance.exports.foo;
    let exception;

    function callback() {
        exception = new WebAssembly.Exception(tag, []);
        throw exception;
    }

    try {
        instance.exports.call();
    } catch (e) {
        assert.eq(exception, e);
    }

}

function loopOSRWithMultiRethrow2() {
    const b = new Builder();
    b.Type().End()
        .Import().Function("context", "callback", { params: [], ret: "void" }).End()
        .Function().End()
        .Exception().Signature({ params: []}).End()
        .Export().Function("call")
            .Exception("foo", 0)
        .End()
        .Code()
            .Function("call", { params: ["i32"], ret: "i32" })
            .Try("i32")
                .Call(0)
                .Unreachable()
            .Catch(0)
                .Try("i32")
                .Call(0)
                .Unreachable()
                .CatchAll()
                    .Loop("i32")

                    .GetLocal(0)
                    .I32Const(1)
                    .I32Add()
                    .TeeLocal(0)

                    .I32Const(1000)
                    .I32LeU()

                    .BrIf(0)
                    .Rethrow(2)
                    .End()
                .End()
            .End()
            .End()
        .End()


    const bin = b.WebAssembly().get();
    const module = new WebAssembly.Module(bin);
    const instance = new WebAssembly.Instance(module, { context: { callback }});

    let tag = instance.exports.foo;
    let exceptions = [];

    function callback() {
        exceptions.push(new WebAssembly.Exception(tag, []));
        throw exceptions[exceptions.length - 1];
    }

    try {
        instance.exports.call();
    } catch (e) {
        assert.eq(exceptions[0], e);
    }

}

loopOSRWithRethrow();
loopOSRWithMultiRethrow1();
loopOSRWithMultiRethrow2();
