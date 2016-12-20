import * as assert from '../assert.js';
import Builder from '../Builder.js';

(function CallIndirectWithDuplicateSignatures() {
    const builder = (new Builder())
        .Type()
            .Func(["i32"], "i32")         // 0
            .Func(["i32"], "i32")         // 1
            .Func(["i32", "i32"], "i32")  // 2
            .Func(["i32"], "i32")         // 3
            .Func(["i32"], "i32")         // 4
            .Func(["i32", "i32"], "i32")  // 5
            .Func(["f64", "f64"], "f64")  // 6
            .Func(["i32"], "f64")         // 7
            .Func(["i32"], "f64")         // 8
        .End()
        .Function().End()
        .Table()
            .Table({initial: 4, maximum: 4, element: "anyfunc"})
        .End()
        .Export()
            .Function("entry")
            .Table("table", 0)
            .Function("callMe")
        .End()
        .Code()
            .Function("entry", 1)
                .I32Const(42)
                .GetLocal(0)
                .I32Add()
                .I32Const(0) // Function index 0.
                .CallIndirect(4, 0) // Different signature index, but same signature.
                .Return()
            .End()
            .Function("callMe", 3)
                .I32Const(3)
                .GetLocal(0)
                .I32Add()
                .Return()
            .End()
        .End();
    const bin = builder.WebAssembly().get();
    const module = new WebAssembly.Module(bin);
    let value0 = undefined;
    const instance = new WebAssembly.Instance(module);
    let table = instance.exports.table;
    table.set(0, instance.exports.callMe);
    assert.eq(instance.exports.entry(5), 5 + 42 + 3);
}());
