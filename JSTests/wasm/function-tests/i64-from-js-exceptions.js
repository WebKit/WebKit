import Builder from '../Builder.js'
import * as assert from '../assert.js'

const builder = (new Builder())
    .Type().End()
    .Import()
        .Function("import", "sideEffects", {params: [], ret: "void"})
    .End()
    .Function().End()
    .Export()
        .Function("foo")
        .Function("bar")
    .End()
    .Code()
        .Function("foo", {params: ["i64"], ret: "void"})
            .Call(0)
            .Return()
        .End()
        .Function("bar", {params: [], ret: "i64"})
            .Call(0)
            .I32Const(25)
            .I64ExtendUI32()
            .Return()
        .End()
    .End();

const bin = builder.WebAssembly().get();
const module = new WebAssembly.Module(bin);
let called = false;
const imp = {
    import: { 
        sideEffects() { called = true; }
    }
};

const instance = new WebAssembly.Instance(module, imp);
assert.throws(() => instance.exports.foo(20), WebAssembly.RuntimeError, "WebAssembly function with an i64 argument can't be called from JavaScript");
assert.throws(() => instance.exports.foo({valueOf() { throw new Error("Should not be called!"); }}), WebAssembly.RuntimeError, "WebAssembly function with an i64 argument can't be called from JavaScript");
assert.throws(() => instance.exports.bar(), WebAssembly.RuntimeError, "WebAssembly function that returns i64 can't be called from JavaScript");
assert.eq(called, false);
