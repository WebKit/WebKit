//@ runWebAssemblySuite("--useWebAssemblyReferences=true")
import * as assert from '../assert.js';
import Builder from '../Builder.js';

const $1 = new WebAssembly.Instance(new WebAssembly.Module((new Builder())
      .Type().End()
      .Function().End()
      .Export()
          .Function("h")
          .Function("i")
          .Function("j")
          .Function("k")
      .End()
      .Code()
        .Function("h", { params: ["anyref"], ret: "anyref" })
          .GetLocal(0)
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
      .End().WebAssembly().get()));

const $2 = new WebAssembly.Instance(new WebAssembly.Module((new Builder())
      .Type().End()
      .Import()
          .Function("m1", "h", { params: ["anyref"], ret: "anyref" })
          .Function("m1", "j", { params: ["anyref"], ret: "i32" })
          .Function("js", "ident", { params: ["anyref"], ret: "anyref" })
          .Function("js", "make_null", { params: [], ret: "anyref" })
      .End()
      .Function().End()
      .Export()
          .Function("call_h")
          .Function("call_j")
          .Function("call_h_null")
          .Function("call_j_null")
          .Function("call_ident")
          .Function("call_ident_null")
          .Function("is_makenull_null")
      .End()
      .Code()
        .Function("call_h", { params: ["anyref"], ret: "anyref" })
          .GetLocal(0)
          .Call(0)
        .End()

        .Function("call_j", { params: ["anyref"], ret: "i32" })
          .GetLocal(0)
          .Call(1)
        .End()

        .Function("call_h_null", { params: [], ret: "anyref" })
          .RefNull()
          .Call(0)
        .End()

        .Function("call_j_null", { params: [], ret: "i32" })
          .RefNull()
          .Call(1)
        .End()

        .Function("call_ident", { params: ["anyref"], ret: "anyref" })
          .I32Const(1)
          .If("anyref")
          .Block("anyref", (b) =>
            b.GetLocal(0)
          )
          .Else()
          .Block("anyref", (b) =>
            b.GetLocal(0)
          )
          .End()
          .Call(2)
        .End()

        .Function("call_ident_null", { params: [], ret: "anyref" })
          .RefNull()
          .Call(2)
        .End()

        .Function("is_makenull_null", { params: [], ret: "i32" })
          .Call(3)
          .RefIsNull()
        .End()
      .End().WebAssembly().get()), { m1: $1.exports, js: {
        ident: function(x) { return x; },
        make_null: function() { return null; },
      } });

assert.eq($2.exports.call_h(null), null)

const obj = { test: "hi" }
assert.eq($2.exports.call_h(obj), obj)
assert.eq($2.exports.call_h(5), 5)
assert.eq($2.exports.call_h("hi"), "hi")
assert.eq($2.exports.call_h(undefined), undefined)

assert.eq($2.exports.call_j(obj), 0)
assert.eq($2.exports.call_j(5), 0)
assert.eq($2.exports.call_j("hi"), 0)
assert.eq($2.exports.call_j(null), 1)
assert.eq($2.exports.call_j(undefined), 0)

assert.eq($2.exports.call_h_null(), null)
assert.eq($2.exports.call_j_null(), 1)

assert.eq($2.exports.call_ident(null), null)
assert.eq($2.exports.call_ident(obj), obj)
assert.eq($2.exports.call_ident(5), 5)
assert.eq($2.exports.call_ident("hi"), "hi")
assert.eq($2.exports.call_ident(undefined), undefined)

for (let i=0; i<1000; ++i) {
    // Trigger the ic path
    assert.eq($2.exports.call_ident(null), null)
    assert.eq($2.exports.call_ident(7), 7)
    assert.eq($2.exports.call_ident("bye"), "bye")
}

assert.eq($2.exports.call_ident_null(), null)
assert.eq($2.exports.is_makenull_null(), 1)
