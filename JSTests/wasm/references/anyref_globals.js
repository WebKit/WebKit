//@ runWebAssemblySuite("--useWebAssemblyReferences=true")
import * as assert from '../assert.js';
import Builder from '../Builder.js';

fullGC()
gc()

const $1 = new WebAssembly.Instance(new WebAssembly.Module((new Builder())
      .Type().End()
      .Import()
           .Global().Anyref("imp", "ref", "immutable").End()
      .End()
      .Function().End()
      .Global()
          .RefNull("anyref", "mutable")
          .RefNull("anyref", "immutable")
          .GetGlobal("anyref", 0, "mutable")
      .End()
      .Export()
          .Function("set_glob")
          .Function("get_glob")
          .Function("glob_is_null")
          .Function("set_glob_null")
          .Function("get_import")
          .Global("expglob", 2)
          .Global("expglob2", 0)
      .End()
      .Code()
        .Function("set_glob", { params: ["anyref"], ret: "void" })
          .GetLocal(0)
          .SetGlobal(1)
        .End()

        .Function("get_glob", { params: [], ret: "anyref" })
            .GetGlobal(1)
        .End()

        .Function("glob_is_null", { params: [], ret: "i32" })
            .Call(1)
            .RefIsNull()
        .End()

        .Function("set_glob_null", { params: [], ret: "void" })
            .RefNull()
            .Call(0)
        .End()

        .Function("get_import", { params: [], ret: "anyref" })
            .GetGlobal(0)
        .End()
      .End().WebAssembly().get()), { imp: { ref: "hi" } });

assert.eq($1.exports.get_import(), "hi")
assert.eq($1.exports.expglob.value, null)
assert.eq($1.exports.expglob2.value, "hi")
assert.eq($1.exports.get_glob(), null)

const obj = { test: "hi" }

assert.throws(() => $1.exports.expglob2 = null, TypeError, "Attempted to assign to readonly property.")

$1.exports.set_glob(obj); assert.eq($1.exports.get_glob(), obj); assert.eq($1.exports.expglob2.value, "hi")
$1.exports.set_glob(5); assert.eq($1.exports.get_glob(), 5)
$1.exports.set_glob(null); assert.eq($1.exports.get_glob(), null)
$1.exports.set_glob("hi"); assert.eq($1.exports.get_glob(), "hi")
$1.exports.set_glob(undefined); assert.eq($1.exports.get_glob(), undefined)
assert.eq($1.exports.glob_is_null(), 0)
$1.exports.set_glob_null(); assert.eq($1.exports.get_glob(), null)
assert.eq($1.exports.glob_is_null(), 1)

const obj2 = { test: 21 }
$1.exports.set_glob(obj2); assert.eq($1.exports.get_glob(), obj2)

$1.exports.get_glob(obj2).test = 5;
assert.eq($1.exports.get_glob().test, 5)
assert.eq(obj2.test, 5)

function doGCSet() {
    fullGC()
    $1.exports.set_glob({ test: -1 })
    fullGC()
}

function doGCTest() {
    for (let i=0; i<1000; ++i) {
        assert.eq($1.exports.get_glob().test, -1)
        fullGC()
    }
}

doGCSet()
doGCTest()

let count = 0

function doBarrierSet() {
    ++count
    $1.exports.set_glob({ test: -count })
}

function doBarrierTest() {
    let garbage = { val: "hi", val2: 5, arr: [] }
    for (let i=0; i<100; ++i) garbage.arr += ({ field: i })

    for (let j=0; j<1000; ++j) {
        assert.eq($1.exports.get_glob().test, -count)
        edenGC()
    }
}

for (let i=0; i<5; ++i) {
    doBarrierSet()
    doBarrierTest()
    doBarrierTest()
}
