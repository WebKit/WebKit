import * as assert from '../assert.js';
import Builder from '../Builder.js';

function checkRefNullWithI32ImmType() {
  /*
  (module
    (func (export "r") (result i32)
      ref.null i32
      ref.is_null
    )
  )
  */
  let bytes = Uint8Array.from([0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x05, 0x01, 0x60, 0x00, 0x01, 0x7f, 0x03, 0x02, 0x01, 0x00, 0x07, 0x05, 0x01, 0x01, 0x72, 0x00, 0x00, 0x0a, 0x07, 0x01, 0x05, 0x00, 0xd0, 0x7f, 0xd1, 0x0b]);
  assert.throws(() => new WebAssembly.Module(bytes), Error, "WebAssembly.Module doesn't parse at byte 3: ref.null type must be a reference type, in function at index 0 (evaluating 'new WebAssembly.Module(bytes)')");
}

checkRefNullWithI32ImmType();

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
        .Function("h", { params: ["externref"], ret: "externref" }, ["externref"])
          .GetLocal(0)
          .SetLocal(1)
          .GetLocal(1)
        .End()

        .Function("i", { params: [], ret: "externref" })
            .RefNull("externref")
            .Call(0)
        .End()

        .Function("j", { params: ["externref"], ret: "i32" })
            .GetLocal(0)
            .RefIsNull()
        .End()

        .Function("k", { params: [], ret: "i32" })
            .RefNull("externref")
            .RefIsNull()
        .End()

        .Function("local_read", { params: [], ret: "i32" }, ["externref"])
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
