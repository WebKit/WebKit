import Builder from '../Builder.js'
import * as assert from '../assert.js'

function makeInstance(func) {
    const builder = new Builder()
        .Type()
            .Func(["i32", "i32"], "i32")
            .Func(["i32"], "i32")
        .End()
        .Import()
            .Table("imp", "table", {initial: 20, element: "anyfunc"})
            .Function("imp", "func", { params: ["i32"], ret: "i32" })
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
                .Call(0)
                .Return()
            .End()
        .End();


    const bin = builder.WebAssembly().get();
    const module = new WebAssembly.Module(bin);
    const table = new WebAssembly.Table({initial: 20, element: "anyfunc"});
    return {instance: new WebAssembly.Instance(module, {imp: {table, func}}), table};
}

{
    let i;
    function func(x) {
        if (x !== i)
            throw new Error("Bad argument");
        return x + 44;
    }
    const {instance, table} = makeInstance(func);
    const exports = instance.exports;
    const foo = exports.foo;
    table.set(0, exports.bar);
    assert.eq(table.get(0), exports.bar);

    for (i = 0; i < 10000; i++) {
        if (foo(0, i) !== i + 44)
            throw new Error("Bad call indirect");
    }
}
