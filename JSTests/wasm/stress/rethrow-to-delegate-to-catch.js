import Builder from '../Builder.js'
import * as assert from '../assert.js'

{
    const b = new Builder();
    b.Type().End()
        .Import()
            .Function("context", "callback", { params: [], ret: "void" })
            .Exception("context", "tag", { params: ["i32"], ret: "void" })
        .End()
        .Function().End()
        .Exception().Signature({ params: []}).End()
        .Export().Function("call")
            .Exception("foo", 1)
        .End()
        .Code()
            .Function("call", { params: [], ret: "i32" })
            .Try("i32")
                .Try("i32")
                    .Try("i32")
                        .Call(0)
                        .I32Const(1)
                    .CatchAll()
                        .I32Const(2)
                        .Rethrow(0)
                    .End()
                .Delegate(0)
            .Catch(1)
                .I32Const(3)
            .End()
            .End()
        .End()


    const bin = b.WebAssembly().get();
    const module = new WebAssembly.Module(bin);
    let otherTag = new WebAssembly.Tag({ parameters: ["i32"]})
    const instance = new WebAssembly.Instance(module, { context: { callback, tag: otherTag }});

    let tag = instance.exports.foo;
    let args = [];

    function callback() {
        throw new WebAssembly.Exception(tag, args);
    }

    assert.eq(instance.exports.call(), 3);

    tag = otherTag;
    args = [100];

    let exn = assert.throws(instance.exports.call, WebAssembly.Exception);
    assert.eq(exn.getArg(tag, 0), 100);
}

{
    const b = new Builder();
    b.Type().End()
        .Import()
            .Function("context", "callback", { params: [], ret: "void" })
            .Exception("context", "tag", { params: ["i32"], ret: "void" })
        .End()
        .Function().End()
        .Exception().Signature({ params: []}).End()
        .Export().Function("call")
            .Exception("foo", 1)
        .End()
        .Code()
            .Function("call", { params: [], ret: "i32" })
            .Try("i32")
                .Try("i32")
                    .Call(0)
                    .I32Const(0)
                .CatchAll()
                    .Try("i32")
                        .Call(0)
                        .I32Const(1)
                    .CatchAll()
                        .I32Const(2)
                        .Rethrow(1)
                    .End()
                .End()
            .Delegate(0)
            .End()
        .End()


    const bin = b.WebAssembly().get();
    const module = new WebAssembly.Module(bin);
    let otherTag = new WebAssembly.Tag({ parameters: ["i32"]})
    const instance = new WebAssembly.Instance(module, { context: { callback, tag: otherTag }});

    let tag = instance.exports.foo;
    let args = [];

    function callback() {
        let exn = new WebAssembly.Exception(tag, args);
        tag = otherTag;
        args = [100];
        throw exn;
    }

    assert.throws(instance.exports.call, WebAssembly.Exception);
}

{
    const b = new Builder();
    b.Type().End()
        .Import()
            .Function("context", "callback", { params: [], ret: "void" })
            .Exception("context", "tag", { params: ["i32"], ret: "void" })
        .End()
        .Function().End()
        .Exception().Signature({ params: []}).End()
        .Export().Function("call")
            .Exception("foo", 1)
        .End()
        .Code()
            .Function("call", { params: [], ret: "i32" })
            .Try("i32")
                .Try("i32")
                    .Try("i32")
                        .Call(0)
                        .I32Const(0)
                    .CatchAll()
                        .Try("i32")
                            .Call(0)
                            .I32Const(1)
                        .CatchAll()
                            .I32Const(2)
                            .Rethrow(1)
                        .End()
                    .End()
                .Delegate(1)
            .CatchAll()
                .I32Const(3)
            .End()
            .End()
        .End()


    const bin = b.WebAssembly().get();
    const module = new WebAssembly.Module(bin);
    let otherTag = new WebAssembly.Tag({ parameters: ["i32"]})
    const instance = new WebAssembly.Instance(module, { context: { callback, tag: otherTag }});

    let tag = instance.exports.foo;
    let args = [];

    function callback() {
        let exn = new WebAssembly.Exception(tag, args);
        tag = otherTag;
        args = [100];
        throw exn;
    }

    assert.throws(instance.exports.call, WebAssembly.Exception);
}


{
    const b = new Builder();
    b.Type().End()
        .Import()
            .Function("context", "callback", { params: [], ret: "void" })
            .Exception("context", "tag", { params: ["i32"], ret: "void" })
        .End()
        .Function().End()
        .Exception().Signature({ params: []}).End()
        .Export().Function("call")
            .Exception("foo", 1)
        .End()
        .Code()
            .Function("call", { params: [], ret: "i32" })
            .Try("i32")
                .Try("i32")
                    .Try("i32")
                        .Call(0)
                        .I32Const(0)
                    .CatchAll()
                        .I32Const(1)
                        .Rethrow(0)
                    .End()
                .Delegate(0)
            .Catch(0)
            .End()
            .End()
        .End()


    const bin = b.WebAssembly().get();
    const module = new WebAssembly.Module(bin);
    let otherTag = new WebAssembly.Tag({ parameters: ["i32"]})
    const instance = new WebAssembly.Instance(module, { context: { callback, tag: otherTag }});

    let tag = otherTag;
    let args = [100];

    function callback() {
        let exn = new WebAssembly.Exception(tag, args);
        throw exn;
    }

    assert.eq(instance.exports.call(), 100);
}

{
    const b = new Builder();
    b.Type().End()
        .Import()
            .Function("context", "callback", { params: [], ret: "void" })
            .Exception("context", "tag", { params: ["i32"], ret: "void" })
        .End()
        .Function().End()
        .Exception().Signature({ params: []}).End()
        .Export().Function("call")
            .Exception("foo", 1)
        .End()
        .Code()
            .Function("call", { params: [], ret: "i32" })
            .Try("i32")
                .Try("i32")
                    .Try("i32")
                        .Call(0)
                        .I32Const(0)
                    .CatchAll()
                        .Try("i32")
                            .Call(0)
                            .I32Const(1)
                        .CatchAll()
                            .I32Const(2)
                            .Rethrow(1)
                        .End()
                    .End()
                .Delegate(0)
            .Catch(1)
                .I32Const(3)
            .End()
            .End()
        .End()


    const bin = b.WebAssembly().get();
    const module = new WebAssembly.Module(bin);
    let otherTag = new WebAssembly.Tag({ parameters: ["i32"]})
    const instance = new WebAssembly.Instance(module, { context: { callback, tag: otherTag }});

    let tag = instance.exports.foo;
    let args = [];

    function callback() {
        let exn = new WebAssembly.Exception(tag, args);
        tag = otherTag;
        args = [100];
        throw exn;
    }

    assert.eq(instance.exports.call(), 3);
}
