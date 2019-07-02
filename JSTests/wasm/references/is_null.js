//@ runWebAssemblySuite("--useWebAssemblyReferences=true")
import * as assert from '../assert.js';
import Builder from '../Builder.js';

const builder = (new Builder())
      .Type().End()
      .Function().End()
      .Export()
          .Function("h")
          .Function("i")
          .Function("j")
          .Function("k")
          .Function("local_read")
      .End()
      .Code()
        .Function("h", { params: ["anyref"], ret: "anyref" }, ["anyref"])
          .GetLocal(0)
          .SetLocal(1)
          .GetLocal(1)
        .End()

        .Function("i", { params: [], ret: "anyref" })
            .RefNull()
            .Call(0)
        .End()

        .Function("j", { params: ["anyref"], ret: "i32" })
            .GetLocal(0)
            .RefIsNull()
        .End()

        .Function("k", { params: [], ret: "i32" })
            .RefNull()
            .RefIsNull()
        .End()

        .Function("local_read", { params: [], ret: "i32" }, ["anyref"])
            .GetLocal(0)
            .RefIsNull()
        .End()
      .End();

const bin = builder.WebAssembly().get();
const module = new WebAssembly.Module(bin);
const instance = new WebAssembly.Instance(module);
assert.eq(instance.exports.h(null), null)

const obj = { test: "hi" }
assert.eq(instance.exports.h(obj), obj)
assert.eq(instance.exports.h(5), 5)
assert.eq(instance.exports.h("hi"), "hi")
assert.eq(instance.exports.h(undefined), undefined)

assert.eq(instance.exports.i(), null)

assert.eq(instance.exports.j(obj), 0)
assert.eq(instance.exports.j(5), 0)
assert.eq(instance.exports.j("hi"), 0)
assert.eq(instance.exports.j(null), 1)
assert.eq(instance.exports.j(undefined), 0)

assert.eq(instance.exports.k(), 1)
assert.eq(instance.exports.local_read(), 1)

assert.eq(obj.test, "hi")
const obj2 = instance.exports.h(obj)
obj2.test = "bye"
assert.eq(obj.test, "bye")

for (let i=0; i<1000; ++i) assert.eq(instance.exports.h(null), null)
