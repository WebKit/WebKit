import Builder from '../Builder.js'
import * as assert from '../assert.js'

function testCannotCatchUnreachable() {
    const b = new Builder();
    b.Type().End()
        .Function().End()
        .Export()
         .Function("call")
        .End()
        .Code()
            .Function("call", { params: [], ret: "i32" })
                .Try("i32")
                    .Unreachable()
                .CatchAll()
                    .I32Const(1)
                .End()
            .End()
        .End()


    const bin = b.WebAssembly().get();
    const module = new WebAssembly.Module(bin);
    const instance = new WebAssembly.Instance(module);

    assert.throws(instance.exports.call, WebAssembly.RuntimeError, "Unreachable code should not be executed");
}

function testCannotCatchOOB() {
    const b = new Builder();
    b.Type().End()
        .Function().End()
        .Memory()
            .InitialMaxPages(0, 1)
        .End()
        .Export()
         .Function("call")
        .End()
        .Code()
            .Function("call", { params: [], ret: "i32" })
                .Try("i32")
                    .I32Const(1024)
                    .I32Load(0, 0)
                .CatchAll()
                    .I32Const(1)
                .End()
            .End()
        .End()


    const bin = b.WebAssembly().get();
    const module = new WebAssembly.Module(bin);
    const instance = new WebAssembly.Instance(module);

    assert.throws(instance.exports.call, WebAssembly.RuntimeError, "Out of bounds memory access");
}

function testWasmAPIThrow() {
    const b = new Builder();
    b.Type().End()
        .Import().Function("context", "callback", { params: [], ret: "void" }).End()
        .Function().End()
        .Export()
            .Function("call")
        .End()
        .Code()
            .Function("call", { params: [], ret: "i32" })
                .Try("i32")
                    .Call(0)
                    .I32Const(0)
                .CatchAll()
                    .I32Const(1)
                .End()
            .End()
        .End()


    const bin = b.WebAssembly().get();
    const module = new WebAssembly.Module(bin);
    const instance = new WebAssembly.Instance(module, { context: { callback }});

    function callback() {
        new WebAssembly.Global({
            value: 'anyfunc'
        }, {});
    }

    assert.eq(instance.exports.call(), 1);
}
function testJSCatchAndRethrow() {
    const b = new Builder();
    b.Type().End()
        .Import().Function("context", "callback", { params: [], ret: "void" }).End()
        .Function().End()
        .Export()
            .Function("call")
        .End()
        .Code()
            .Function("call", { params: ["i32"], ret: "i32" })
                .Try("i32")
                    .GetLocal(0)
                    .I32Eqz()
                    .If("void")
                        .Unreachable()
                    .Else()
                        .Call(0)
                    .End()
                    .I32Const(0)
                .CatchAll()
                    .I32Const(1)
                .End()
            .End()
        .End()


    const bin = b.WebAssembly().get();
    const module = new WebAssembly.Module(bin);
    const instance = new WebAssembly.Instance(module, { context: { callback }});

    function callback() {
        try {
            instance.exports.call(0);
        } catch(e) {
            throw e;
        }
    }

    assert.throws(() => instance.exports.call(1), WebAssembly.RuntimeError, "Unreachable code should not be executed");
}

testCannotCatchUnreachable();
testCannotCatchOOB();
testWasmAPIThrow();
testJSCatchAndRethrow();
