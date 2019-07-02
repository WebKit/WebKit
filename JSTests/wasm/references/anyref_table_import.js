//@ runWebAssemblySuite("--useWebAssemblyReferences=true")
import * as assert from '../assert.js';
import Builder from '../Builder.js';

{
    function makeImport() {
        const tbl = new WebAssembly.Table({initial:2, element:"anyref"});

        const $1 = new WebAssembly.Instance(new WebAssembly.Module((new Builder())
          .Type().End()
          .Import()
                .Table("imp", "tbl", {initial: 2, element: "anyref"})
          .End()
          .Function().End()
          .Export()
              .Function("set_tbl")
              .Function("get_tbl")
              .Function("tbl_is_null")
              .Function("set_tbl_null")
              .Table("tbl", 0)
          .End()
          .Code()
            .Function("set_tbl", { params: ["anyref"], ret: "void" })
              .I32Const(0)
              .GetLocal(0)
              .TableSet(0)
            .End()

            .Function("get_tbl", { params: [], ret: "anyref" })
              .I32Const(0)
              .TableGet(0)
            .End()

            .Function("tbl_is_null", { params: [], ret: "i32" })
                .Call(1)
                .RefIsNull()
            .End()

            .Function("set_tbl_null", { params: [], ret: "void" })
                .RefNull()
                .Call(0)
            .End()
          .End().WebAssembly().get()), { imp: { tbl }});
        fullGC()
        return $1
    }

    const $1 = makeImport()
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

    assert.eq($1.exports.tbl.grow(10), 2)
    assert.eq($1.exports.tbl.grow(0), 12)
    assert.eq($1.exports.get_tbl().test, "test")
    fullGC()
    assert.eq($1.exports.get_tbl().test, "test")
}

{
    function makeImport() {
        const $1 = new WebAssembly.Instance(new WebAssembly.Module((new Builder())
          .Type().End()
          .Import()
                .Table("imp", "tbl", {initial: 2, element: "anyref"})
          .End()
          .Function().End()
          .Export()
              .Function("set_tbl")
              .Function("get_tbl")
              .Function("tbl_is_null")
              .Function("set_tbl_null")
              .Table("tbl", 0)
          .End()
          .Code()
            .Function("set_tbl", { params: ["anyref"], ret: "void" })
              .I32Const(0)
              .GetLocal(0)
              .TableSet(0)
            .End()

            .Function("get_tbl", { params: [], ret: "anyref" })
              .I32Const(0)
              .TableGet(0)
            .End()

            .Function("tbl_is_null", { params: [], ret: "i32" })
                .Call(1)
                .RefIsNull()
            .End()

            .Function("set_tbl_null", { params: [], ret: "void" })
                .RefNull()
                .Call(0)
            .End()
          .End().WebAssembly().get()), { imp: { tbl: new WebAssembly.Table({initial:2, element:"anyref"}) }});
        fullGC()

        $1.exports.tbl.set(0, { test: "test" });

        const $2 = new WebAssembly.Instance(new WebAssembly.Module((new Builder())
          .Type().End()
          .Import()
                .Table("imp", "tbl", {initial: 2, element: "anyref"})
          .End()
          .Function().End()
          .Export()
              .Function("set_tbl")
              .Function("get_tbl")
              .Function("tbl_is_null")
              .Function("set_tbl_null")
              .Table("tbl", 0)
          .End()
          .Code()
            .Function("set_tbl", { params: ["anyref"], ret: "void" })
              .I32Const(0)
              .GetLocal(0)
              .TableSet(0)
            .End()

            .Function("get_tbl", { params: [], ret: "anyref" })
              .I32Const(0)
              .TableGet(0)
            .End()

            .Function("tbl_is_null", { params: [], ret: "i32" })
                .Call(1)
                .RefIsNull()
            .End()

            .Function("set_tbl_null", { params: [], ret: "void" })
                .RefNull()
                .Call(0)
            .End()
          .End().WebAssembly().get()), { imp: { tbl: $1.exports.tbl }});
        fullGC()

        return $2
    }

    const $1 = makeImport()
    fullGC()

    assert.eq($1.exports.get_tbl().test, "test")
    assert.eq($1.exports.tbl_is_null(), 0)

    $1.exports.set_tbl("hi")
    fullGC()
    assert.eq($1.exports.get_tbl(), "hi")
    assert.eq($1.exports.tbl_is_null(), 0)

    assert.eq($1.exports.tbl.get(0), "hi")
    assert.eq($1.exports.tbl.get(1), null)

    $1.exports.tbl.set(0, { test: "test" });
    fullGC()
    assert.eq($1.exports.get_tbl().test, "test")

    assert.eq($1.exports.tbl.grow(10), 2)
    assert.eq($1.exports.tbl.grow(0), 12)
    assert.eq($1.exports.get_tbl().test, "test")
    fullGC()
    assert.eq($1.exports.get_tbl().test, "test")
}

{
    let tbl = new WebAssembly.Table({initial:1, element:"anyref"});

    function doSet(i, v) {
        tbl.set(i, { test: v });
    }

    function makeGarbage(depth) {
        let garbage = { val: "hi", val2: 5, arr: [] }
        for (let i=0; i<100; ++i) garbage.arr += ({ field: i })

        if (depth < 5)
            return 1 + makeGarbage(depth + 1);
        return 0;
    }

    for (let iter=0; iter<5; ++iter) {
        for (let k=0; k<100; ++k)
            tbl = new WebAssembly.Table({initial:100, element:"anyref"})

        for (let i=1; i<20; ++i) {
            const len = tbl.length;
            for (let j=0; j<len; ++j)
                doSet(j, j);
            makeGarbage(0);
            tbl.grow(Math.pow(2,i));
            for (let j=0; j<len; ++j)
                assert.eq(tbl.get(j).test, j);
            for (let j=0; j<tbl.length; ++j)
                doSet(j, -j);
        }
    }
}

{
    const tbl = new WebAssembly.Table({initial:2, element:"funcref"});

    const mod = new WebAssembly.Module((new Builder())
      .Type().End()
      .Import()
            .Table("imp", "tbl", {initial: 2, element: "anyref"})
      .End()
      .Function().End()
      .Export()
      .End()
      .Code()
      .End().WebAssembly().get())

    assert.throws(() => new WebAssembly.Instance(mod, { imp: { tbl }}), Error, "Table import imp:tbl provided a 'type' that is wrong (evaluating 'new WebAssembly.Instance(mod, { imp: { tbl }})')");
}

{
    const tbl = new WebAssembly.Table({initial:2, element:"funcref"});

    const mod = new WebAssembly.Module((new Builder())
      .Type().End()
      .Import()
            .Table("imp", "tbl", {initial: 2, element: "anyref"})
      .End()
      .Function().End()
      .Export()
      .End()
      .Code()
      .End().WebAssembly().get())

    assert.throws(() => new WebAssembly.Instance(mod, { imp: { tbl }}), Error, "Table import imp:tbl provided a 'type' that is wrong (evaluating 'new WebAssembly.Instance(mod, { imp: { tbl }})')");
}
