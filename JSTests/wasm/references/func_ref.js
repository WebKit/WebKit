//@ runWebAssemblySuite("--useWebAssemblyReferences=true")
import * as assert from '../assert.js';
import Builder from '../Builder.js';

fullGC()
gc()

function makeExportedFunction(i) {
    const builder = (new Builder())
          .Type().End()
          .Function().End()
          .Export()
              .Function("h")
          .End()
          .Code()
            .Function("h", { params: [], ret: "i32" }, [])
              .I32Const(i)
            .End()
          .End();

    const bin = builder.WebAssembly().get();
    const module = new WebAssembly.Module(bin);
    const instance = new WebAssembly.Instance(module);

    return instance.exports.h
}

function makeExportedIdent() {
    const builder = (new Builder())
          .Type().End()
          .Function().End()
          .Export()
              .Function("h")
          .End()
          .Code()
            .Function("h", { params: ["i32"], ret: "i32" }, [])
              .GetLocal(0)
            .End()
          .End();

    const bin = builder.WebAssembly().get();
    const module = new WebAssembly.Module(bin);
    const instance = new WebAssembly.Instance(module);

    return instance.exports.h
}

function makeFuncrefIdent() {
    const builder = (new Builder())
          .Type().End()
          .Function().End()
          .Export()
              .Function("h")
          .End()
          .Code()
            .Function("h", { params: ["funcref"], ret: "funcref" }, [])
              .GetLocal(0)
            .End()
          .End();

    const bin = builder.WebAssembly().get();
    const module = new WebAssembly.Module(bin);
    const instance = new WebAssembly.Instance(module);

    return instance.exports.h
}

{
    const myfun = makeExportedFunction(1337);
    function fun() {
        return 41;
    }

    const builder = (new Builder())
          .Type().End()
          .Function().End()
          .Export()
              .Function("h")
              .Function("i")
              .Function("get_h")
              .Function("fix")
              .Function("get_not_exported")
              .Function("local_read")
          .End()
          .Code()
            .Function("h", { params: ["funcref"], ret: "anyref" }, ["anyref"])
              .GetLocal(0)
              .SetLocal(1)
              .GetLocal(1)
            .End()

            .Function("i", { params: ["funcref"], ret: "funcref" }, ["funcref"])
              .GetLocal(0)
              .SetLocal(1)
              .GetLocal(1)
            .End()

            .Function("get_h", { params: [], ret: "funcref" }, ["funcref"])
              .I32Const(0)
              .RefFunc(0)
              .SetLocal(0)
              .If("funcref")
              .Block("funcref", (b) =>
                b.GetLocal(0)
              )
              .Else()
              .Block("funcref", (b) =>
                b.GetLocal(0)
              )
              .End()
            .End()

            .Function("fix", { params: [], ret: "funcref" }, [])
              .RefFunc(3)
            .End()

            .Function("get_not_exported", { params: [], ret: "funcref" }, [])
              .RefFunc(5)
            .End()

            .Function("ret_42", { params: [], ret: "i32" }, [])
              .I32Const(42)
            .End()

            .Function("local_read", { params: [], ret: "i32" }, ["funcref"])
              .GetLocal(0)
              .RefIsNull()
            .End()
          .End();

    const bin = builder.WebAssembly().get();
    const module = new WebAssembly.Module(bin);
    const instance = new WebAssembly.Instance(module);
    fullGC();

    assert.eq(instance.exports.local_read(), 1)
    assert.eq(instance.exports.h(null), null)

    assert.throws(() => instance.exports.h(fun), Error, "Funcref must be an exported wasm function (evaluating 'func(...args)')")
    assert.eq(instance.exports.h(myfun), myfun)
    assert.throws(() => instance.exports.h(5), Error, "Funcref must be an exported wasm function (evaluating 'func(...args)')")
    assert.throws(() => instance.exports.h(undefined), Error, "Funcref must be an exported wasm function (evaluating 'func(...args)')")

    assert.eq(instance.exports.i(null), null)
    assert.eq(instance.exports.i(myfun), myfun)
    assert.throws(() => instance.exports.i(fun), Error, "Funcref must be an exported wasm function (evaluating 'func(...args)')")
    assert.throws(() => instance.exports.i(5), Error, "Funcref must be an exported wasm function (evaluating 'func(...args)')")

    assert.throws(() => instance.exports.get_h()(fun), Error, "Funcref must be an exported wasm function (evaluating 'func(...args)')")
    assert.eq(instance.exports.get_h()(null), null)
    assert.eq(instance.exports.get_h()(myfun), myfun)
    assert.throws(() => instance.exports.get_h()(5), Error, "Funcref must be an exported wasm function (evaluating 'func(...args)')")

    assert.eq(instance.exports.get_not_exported()(), 42)

    assert.eq(instance.exports.fix()(), instance.exports.fix());
    assert.eq(instance.exports.fix(), instance.exports.fix);
}

// Globals

{
    const myfun = makeExportedFunction(42);
    function fun() {
        return 41;
    }

    const $1 = (() => new WebAssembly.Instance(new WebAssembly.Module((new Builder())
      .Type().End()
      .Import()
           .Global().Funcref("imp", "ref", "immutable").End()
      .End()
      .Function().End()
      .Global()
          .RefNull("funcref", "mutable")
          .RefNull("funcref", "immutable")
          .GetGlobal("funcref", 0, "mutable")
          .RefFunc("funcref", 2, "immutable")
      .End()
      .Export()
          .Function("set_glob")
          .Function("get_glob")
          .Function("glob_is_null")
          .Function("set_glob_null")
          .Function("get_import")
          .Global("expglob", 2)
          .Global("expglob2", 0)
          .Global("exp_glob_is_null", 4)
      .End()
      .Code()
        .Function("set_glob", { params: ["funcref"], ret: "void" })
          .GetLocal(0)
          .SetGlobal(1)
        .End()

        .Function("get_glob", { params: [], ret: "funcref" })
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

        .Function("get_import", { params: [], ret: "funcref" })
            .GetGlobal(0)
        .End()
      .End().WebAssembly().get()), { imp: { ref: makeExportedFunction(1337) } }))();

    fullGC();

    assert.eq($1.exports.get_import()(), 1337)
    assert.eq($1.exports.expglob, null)
    assert.eq($1.exports.expglob2(), 1337)
    assert.eq($1.exports.exp_glob_is_null, $1.exports.glob_is_null);
    assert.eq($1.exports.get_glob(), null)

    $1.exports.set_glob(myfun); assert.eq($1.exports.get_glob(), myfun); assert.eq($1.exports.get_glob()(), 42); assert.eq($1.exports.expglob2(), 1337)
    $1.exports.set_glob(null); assert.eq($1.exports.get_glob(), null)
    $1.exports.set_glob(myfun); assert.eq($1.exports.get_glob()(), 42);

    assert.throws(() => $1.exports.set_glob(fun), Error, "Funcref must be an exported wasm function (evaluating 'func(...args)')")

    assert.eq($1.exports.glob_is_null(), 0)
    $1.exports.set_glob_null(); assert.eq($1.exports.get_glob(), null)
    assert.eq($1.exports.glob_is_null(), 1)
}

assert.throws(() => new WebAssembly.Instance(new WebAssembly.Module((new Builder())
  .Type().End()
  .Import()
       .Global().Funcref("imp", "ref", "immutable").End()
  .End()
  .Function().End()
  .Code().End().WebAssembly().get()), { imp: { ref: function() { return "hi" } } }), Error, "imported global imp:ref must be a wasm exported function or null (evaluating 'new WebAssembly.Instance')");

assert.throws(() => new WebAssembly.Module((new Builder())
  .Type().End()
  .Function().End()
  .Code()
    .Function("h", { params: ["anyref"], ret: "funcref" })
      .GetLocal(0)
    .End()
  .End().WebAssembly().get()), Error, "WebAssembly.Module doesn't validate: control flow returns with unexpected type, in function at index 0 (evaluating 'new WebAssembly.Module')");

assert.throws(() => new WebAssembly.Module((new Builder())
  .Type().End()
  .Function().End()
  .Table()
    .Table({initial: 1, element: "funcref"})
  .End()
  .Code()
    .Function("h", { params: ["i32"], ret: "void" })
      .GetLocal(0)
      .I32Const(0)
      .TableSet(0)
    .End()
  .End().WebAssembly().get()), Error, "WebAssembly.Module doesn't validate: table.set value to type I32 expected Funcref, in function at index 0 (evaluating 'new WebAssembly.Module')");

// Tables
{
    const $1 = new WebAssembly.Instance(new WebAssembly.Module((new Builder())
      .Type().End()
      .Function().End()
      .Table()
            .Table({initial: 0, element: "anyref"})
            .Table({initial: 1, element: "funcref"})
      .End()
      .Global()
          .RefNull("funcref", "mutable")
      .End()
      .Export()
          .Function("set_glob")
          .Function("get_glob")
          .Function("call_glob")
          .Function("ret_20")
      .End()
      .Code()
        .Function("set_glob", { params: ["funcref"], ret: "void" })
          .GetLocal(0)
          .SetGlobal(0)
        .End()

        .Function("get_glob", { params: [], ret: "funcref" })
            .GetGlobal(0)
        .End()

        .Function("call_glob", { params: ["i32"], ret: "i32" })
            .I32Const(0)
            .GetGlobal(0)
            .TableSet(1)

            .GetLocal(0)
            .I32Const(0)
            .CallIndirect(2,1)
        .End()

        .Function("ret_20", { params: ["i32"], ret: "i32" })
            .I32Const(20)
        .End()
      .End().WebAssembly().get()));

    const myfun = makeExportedFunction(1337);
    function fun(i) {
        return 41;
    }
    const ident = makeExportedIdent();

    $1.exports.set_glob($1.exports.ret_20); assert.eq($1.exports.get_glob(), $1.exports.ret_20); assert.eq($1.exports.call_glob(42), 20)
    $1.exports.set_glob(null); assert.eq($1.exports.get_glob(), null); assert.throws(() => $1.exports.call_glob(42), Error, "call_indirect to a null table entry (evaluating 'func(...args)')")
    $1.exports.set_glob(ident); assert.eq($1.exports.get_glob(), ident); assert.eq($1.exports.call_glob(42), 42)

    assert.throws(() => $1.exports.set_glob(fun), Error, "Funcref must be an exported wasm function (evaluating 'func(...args)')")
    $1.exports.set_glob(myfun); assert.eq($1.exports.get_glob(), myfun); assert.throws(() => $1.exports.call_glob(42), Error, "call_indirect to a signature that does not match (evaluating 'func(...args)')")

    for (let i=0; i<1000; ++i) {
        assert.throws(() => $1.exports.set_glob(function() {}), Error, "Funcref must be an exported wasm function (evaluating 'func(...args)')");
    }
}

// Table set/get

{
    const $1 = new WebAssembly.Instance(new WebAssembly.Module((new Builder())
      .Type().End()
      .Function().End()
      .Table()
            .Table({initial: 0, element: "anyref"})
            .Table({initial: 1, element: "funcref"})
      .End()
      .Export()
          .Function("set")
          .Function("get")
      .End()
      .Code()
        .Function("set", { params: ["funcref"], ret: "void" })
          .I32Const(0)
          .GetLocal(0)
          .TableSet(1)
        .End()

        .Function("get", { params: [], ret: "funcref" })
            .I32Const(0)
            .TableGet(1)
        .End()
      .End().WebAssembly().get()));

    function doSet() {
        const myfun = makeExportedFunction(444);
        for (let i=0; i<1000; ++i) {
            $1.exports.set(myfun);
        }
        $1.exports.set(myfun); assert.eq($1.exports.get(), myfun); assert.eq($1.exports.get()(), 444);
    }

    function doTest(j,k, l) {
        fullGC();
        let garbage = { val: "hi", val2: 5, arr: [] }
        for (let i=0; i<100; ++i) garbage.arr += ({ field: i + j + k + l })
        fullGC();

        for (let i=0; i<100; ++i) {
            assert.eq($1.exports.get()(), 444);
            fullGC();
        }
    }


    doSet()
    doTest(0,0,0)
}

// Wasm->JS Calls

for (let importedFun of [function(i) { return i; }, makeFuncrefIdent()]) {
    const $1 = new WebAssembly.Instance(new WebAssembly.Module((new Builder())
      .Type().End()
      .Import()
           .Function("imp", "h", { params: ["funcref"], ret: "funcref" })
      .End()
      .Function().End()
      .Table()
            .Table({initial: 1, element: "funcref"})
      .End()
      .Export()
          .Function("test1")
          .Function("test2")
          .Function("test3")
          .Function("test4")
      .End()
      .Code()
        .Function("test1", { params: ["funcref"], ret: "funcref" })
          .GetLocal(0)
          .Call(0)
        .End()

        .Function("test2", { params: [], ret: "funcref" })
          .RefFunc(1)
          .Call(0)
        .End()

        .Function("test3", { params: ["funcref"], ret: "funcref" })
          .GetLocal(0)
          .I32Const(0)
          .RefFunc(0)
          .TableSet(0)
          .I32Const(0)
          .CallIndirect(0, 0)
        .End()

        .Function("test4", { params: [], ret: "funcref" })
          .RefFunc(1)
          .I32Const(0)
          .RefFunc(0)
          .TableSet(0)
          .I32Const(0)
          .CallIndirect(0, 0)
        .End()
      .End().WebAssembly().get()), { imp: { h: importedFun } });

    const myfun = makeExportedFunction(1337);
    function fun(i) {
        return 41;
    }

    for (let test of [$1.exports.test1, $1.exports.test3]) {
        assert.eq(test(myfun), myfun)
        assert.eq(test(myfun)(), 1337)
        assert.throws(() => test(fun), Error, "Funcref must be an exported wasm function (evaluating 'func(...args)')")

        for (let i=0; i<1000; ++i) {
            assert.throws(() => test(fun), Error, "Funcref must be an exported wasm function (evaluating 'func(...args)')")
        }
    }

    for (let test of [$1.exports.test2, $1.exports.test4]) {
        assert.eq(test(), $1.exports.test1)
        assert.eq(test()(myfun), myfun)
        assert.throws(() => test()(fun), Error, "Funcref must be an exported wasm function (evaluating 'func(...args)')")

        for (let i=0; i<1000; ++i) {
            assert.throws(() => test()(fun), Error, "Funcref must be an exported wasm function (evaluating 'func(...args)')")
        }
    }
}

{
    const $1 = new WebAssembly.Instance(new WebAssembly.Module((new Builder())
      .Type().End()
      .Import().End()
      .Function().End()
      .Export()
          .Function("test")
      .End()
      .Code()
        .Function("test", { params: ["i32", "funcref"], ret: "i32" })
          .GetLocal(0)
        .End()
      .End().WebAssembly().get()), { });

    const myfun = makeExportedFunction(1337);
    assert.eq(myfun(), 1337)
    assert.eq(42, $1.exports.test(42, myfun))
    assert.throws(() => $1.exports.test(42, () => 5), Error, "Funcref must be an exported wasm function (evaluating 'func(...args)')")
}

{
    const $1 = new WebAssembly.Instance(new WebAssembly.Module((new Builder())
      .Type().End()
      .Import().End()
      .Function().End()
      .Export()
          .Function("test")
      .End()
      .Code()
        .Function("test", { params: ["i32", "funcref"], ret: "i32" })
          .GetLocal(0)
          .If("i32")
          .Block("i32", (b) =>
            b.GetLocal(0)
            .GetLocal(1)
            .Call(0)
          )
          .Else()
          .Block("i32", (b) =>
            b.GetLocal(0)
          )
          .End()
        .End()
      .End().WebAssembly().get()), { });

    const myfun = makeExportedFunction(1337);
    function foo(b) {
        $1.exports.test(b, myfun)
    }
    noInline(foo);

    for (let i = 0; i < 100; ++i)
        foo(0);

    assert.throws(() => $1.exports.test(42, () => 5), Error, "Funcref must be an exported wasm function (evaluating 'func(...args)')")
    assert.throws(() => $1.exports.test(42, myfun), RangeError, "Maximum call stack size exceeded.")
    assert.throws(() => foo(1), RangeError, "Maximum call stack size exceeded.")
}
