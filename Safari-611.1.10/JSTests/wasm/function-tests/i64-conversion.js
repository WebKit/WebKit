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
assert.throws(() => instance.exports.foo(20), TypeError, "Invalid argument type in ToBigInt operation");
assert.eq(instance.exports.foo(20n), undefined);
assert.truthy(called);
called = false;
let convertCalled = false;
assert.eq(instance.exports.foo({valueOf() { convertCalled = true; return 20n; }}), undefined);
assert.truthy(convertCalled);
assert.truthy(called);
called = false;
assert.eq(instance.exports.bar(), 25n);
assert.truthy(called, false);
