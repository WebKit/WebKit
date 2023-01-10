import * as assert from '../assert.js';
import Builder from '../Builder.js';

const $1 = new WebAssembly.Instance(new WebAssembly.Module((new Builder())
      .Type().End()
      .Function().End()
      .Table()
            .Table({initial: 20, maximum: 30, element: "externref"})
      .End()
      .Export()
          .Function("set_tbl")
          .Function("get_tbl")
          .Function("tbl_is_null")
          .Function("set_tbl_null")
          .Table("tbl", 0)
      .End()
      .Code()
        .Function("set_tbl", { params: ["externref"], ret: "void" })
          .I32Const(0)
          .GetLocal(0)
          .TableSet(0)
        .End()

        .Function("get_tbl", { params: [], ret: "externref" })
          .I32Const(0)
          .TableGet(0)
        .End()

        .Function("tbl_is_null", { params: [], ret: "i32" })
            .Call(1)
            .RefIsNull()
        .End()

        .Function("set_tbl_null", { params: [], ret: "void" })
            .RefNull("externref")
            .Call(0)
        .End()
      .End().WebAssembly().get()));

fullGC()

assert.eq($1.exports.get_tbl(), null)
assert.eq($1.exports.tbl_is_null(), 1)

$1.exports.set_tbl("hi")
fullGC()
assert.eq($1.exports.get_tbl(), "hi")
assert.eq($1.exports.tbl_is_null(), 0)

assert.eq($1.exports.tbl.get(0), "hi")
assert.eq($1.exports.tbl.get(1), null)

$1.exports.tbl.set(0, { test: "test" });
fullGC()
assert.eq($1.exports.get_tbl().test, "test")

assert.eq($1.exports.tbl.grow(10), 20)
assert.eq($1.exports.tbl.grow(0), 30)
assert.eq($1.exports.get_tbl().test, "test")
fullGC()
assert.eq($1.exports.get_tbl().test, "test")

assert.throws(() => new WebAssembly.Instance(new WebAssembly.Module((new Builder())
    .Type().End()
    .Function().End()
    .Table()
        .Table({initial: 3, maximum: 3, element: "externref"})
    .End()
    .Element()
        .Element({tableIndex: 0, offset: 0, functionIndices: [0]})
    .End()
    .Code()
    .Function("ret42", { params: [], ret: "i32" })
      .I32Const(42)
    .End()
    .End().WebAssembly().get())), Error, "WebAssembly.Module doesn't parse at byte 30: Table 0 must have type 'Funcref' to have an element section (evaluating 'new WebAssembly.Module')")

function doGCSet() {
    fullGC()
    $1.exports.set_tbl({ test: -1 })
    fullGC()
}

function doGCTest() {
    for (let i=0; i<1000; ++i) {
        assert.eq($1.exports.get_tbl().test, -1)
        fullGC()
    }
}

doGCSet()
doGCTest()

let count = 0

function doBarrierSet() {
    ++count
    $1.exports.set_tbl({ test: -count })
}

function doBarrierTest() {
    let garbage = { val: "hi", val2: 5, arr: [] }
    for (let i=0; i<100; ++i) garbage.arr += ({ field: i })

    for (let j=0; j<1000; ++j) {
        assert.eq($1.exports.get_tbl().test, -count)
        edenGC()
    }
}

for (let i=0; i<5; ++i) {
    doBarrierSet()
    doBarrierTest()
    doBarrierTest()
}
