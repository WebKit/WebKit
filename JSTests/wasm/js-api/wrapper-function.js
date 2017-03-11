import Builder from '../Builder.js';
import * as assert from '../assert.js';


function exportImport(type) {
    let builder = (new Builder())
        .Type().End()
        .Import()
            .Function("imp", "f", type)
        .End()
        .Function().End()
        .Export()
            .Function("func", {module: "imp", field: "f"})
        .End()
        .Code().End();
    return new WebAssembly.Module(builder.WebAssembly().get());
}

{
    let type = { params: ["i32"], ret: "i32" };
    let module = exportImport(type);
    let called = false;
    let foo = (i) => {
        called = true;
        return i + 42;
    };
    let instance = new WebAssembly.Instance(module, {imp: {f: foo}});
    assert.truthy(instance.exports.func !== foo);
    for (let i = 0; i < 100; i++) {
        let r1 = instance.exports.func(i);
        assert.truthy(called);
        called = false;
        let r2 = foo(i);
        called = false;
        assert.eq(r1, r2);
    }

    {
        let builder = (new Builder())
            .Type().End()
            .Import()
                .Function("imp", "f", {params: []})
            .End()
            .Function().End()
            .Code().End();
        let module = new WebAssembly.Module(builder.WebAssembly().get());
        // This should not type check.
        assert.throws(() => new WebAssembly.Instance(module, {imp: {f: instance.exports.func}}), WebAssembly.LinkError, "imported function's signature doesn't match the provided WebAssembly function's signature");
    }

}

{
    const tableDescription = {element: "anyfunc", initial: 2};
    function makeInstance(type, imp) {
        const builder = new Builder()
            .Type()
                .Func(["i32"], "i32")
                .Func(["i32", "i32"], "i32")
            .End()
            .Import()
                .Table("imp", "table", tableDescription)
                .Function("imp", "f1", {params: ["i32"], ret:"i32"})
                .Function("imp", "f2", {params: ["i32", "i32"], ret:"i32"})
            .End()
            .Function().End()
            .Export()
                .Function("foo")
            .End()
            .Element()
                .Element({offset: 0, functionIndices: [0, 1]})
            .End()
            .Code()
                .Function("foo", 1)
                    .GetLocal(1) // parameter to call
                    .GetLocal(0) // call index
                    .CallIndirect(0, 0) // calling function of type ['i32'] => 'i32'
                    .Return()
                .End()
            .End();
        let module = new WebAssembly.Module(builder.WebAssembly().get());
        return new WebAssembly.Instance(module, imp);
    }

    function Bar(){};
    noInline(Bar);
    let called = false;
    let foo = (i) => {
        called = true;
        new Bar;
        return i*42;
    }
    let table = new WebAssembly.Table(tableDescription);
    let inst = makeInstance({params:['i32'], ret:"i32"}, {imp: {f1: foo, f2:foo, table}});
    for (let i = 0; i < 1000; i++) {
        let r1 = inst.exports.foo(0, i);
        assert.truthy(called);
        called = false;
        let r2 = foo(i);
        assert.truthy(called);
        called = false;
        assert.eq(r1, r2);
    }
    for (let i = 0; i < 1000; i++) {
        assert.throws(() => inst.exports.foo(1, i), WebAssembly.RuntimeError, "call_indirect to a signature that does not match");
        assert.truthy(!called);
    }
    for (let i = 0; i < 1000; i++) {
        let r1 = table.get(0)(i);
        table.set(0, table.get(0)); // just make sure setting a wrapper function works.
        assert.truthy(called);
        called = false;
        let r2 = table.get(1)(i);
        assert.truthy(called);
        called = false;
        assert.eq(r1, r2);
    }

    {
        let nextInst = makeInstance({params:['i32'], ret:"i32"}, {imp: {f1: table.get(0), f2:inst.exports.foo, table}});
        for (let i = 0; i < 1000; i++) {
            let r1 = nextInst.exports.foo(0, i);
            assert.truthy(called);
            called = false;
            let r2 = foo(i);
            assert.truthy(called);
            called = false;
            assert.eq(r1, r2);
        }

        for (let i = 0; i < 1000; i++) {
            assert.throws(() => nextInst.exports.foo(1, i), WebAssembly.RuntimeError, "call_indirect to a signature that does not match");
            assert.truthy(!called);
        }

        for (let i = 0; i < 1000; i++) {
            let r1 = table.get(1)(0, i);
            assert.truthy(called);
            called = false;
            let r2 = foo(i);
            assert.truthy(called);
            called = false;
            assert.eq(r1, r2);
        }
    }
}
