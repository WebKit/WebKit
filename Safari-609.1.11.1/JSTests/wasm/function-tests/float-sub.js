import * as assert from '../assert.js'
import Builder from '../Builder.js'

const b = new Builder();
b.Type().End()
    .Function().End()
    .Export()
        .Function("foo")
        .Function("bar")
    .End()
    .Code()
    .Function("bar", { params: ["f32", "f32"], ret: "f32" }, [])
    .GetLocal(0)
    .GetLocal(1)
    .F32Sub()
    .Return()
    .End()

    .Function("foo", { params: ["f32", "f32"], ret: "f32" }, [])
    .GetLocal(0)
    .GetLocal(1)
    .Call(0)
    .Return()
    .End()
    .End()

const bin = b.WebAssembly()
bin.trim();
const instance = new WebAssembly.Instance(new WebAssembly.Module(bin.get()));

let x = new Float32Array(3);
x[0] = 0;
x[1] = 1.5;
x[2] = x[0] - x[1];
assert.eq(instance.exports.bar(x[0], x[1]), x[2]);
assert.eq(instance.exports.foo(x[0], x[1]), x[2]);

x[0] = 100.1234
x[1] = 12.5;
x[2] = x[0] - x[1];
assert.eq(instance.exports.bar(x[0], x[1]), x[2]);
assert.eq(instance.exports.foo(x[0], x[1]), x[2]);
