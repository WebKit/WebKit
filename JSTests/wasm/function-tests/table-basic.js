import Builder from '../Builder.js'
import * as assert from '../assert.js'

function makeInstance() {
    const builder = new Builder()
        .Type()
            .Func(["i32", "i32"], "i32")
            .Func(["i32"], "i32")
        .End()
        .Import()
            .Table("imp", "table", {initial: 20, element: "anyfunc"})
        .End()
        .Function().End()
        .Export()
            .Function("foo")
            .Function("bar")
        .End()
        .Code()
            .Function("foo", 0 /*['i32', 'i32'] => 'i32'*/)
                .GetLocal(1) // parameter to call
                .GetLocal(0) // call index
                .CallIndirect(1, 0) // calling function of type ['i32'] => 'i32'
                .Return()
            .End()
            .Function("bar", 1 /*['i32'] => 'i32'*/)
                .GetLocal(0)
                .I32Const(42)
                .I32Add()
                .Return()
            .End()
        .End();


    const bin = builder.WebAssembly().get();
    const module = new WebAssembly.Module(bin);
    const table = new WebAssembly.Table({initial: 20, element: "anyfunc"});
    return {instance: new WebAssembly.Instance(module, {imp: {table}}), table};
}

{
    const {instance, table} = makeInstance();
    const exports = instance.exports;
    const foo = exports.foo;
    table.set(0, exports.bar);
    assert.eq(table.get(0), exports.bar);

    for (let i = 0; i < 1000; i++)
        assert.eq(foo(0, i), i + 42, "call_indirect");
}

{
    const {instance, table} = makeInstance();
    const foo = instance.exports.foo;
    table.set(0, makeInstance().instance.exports.bar); // Cross instance function.

    for (let i = 0; i < 1000; i++)
        assert.eq(foo(0, i), i + 42, "call_indirect");
}
