import * as assert from '../assert.js';
import Builder from '../Builder.js';

{
    const builder = (new Builder())
      .Type().End()
      .Function().End()
      .Export()
          .Function("j")
      .End()
      .Code()
        .Function("j", { params: [], ret: "i32" })
            .I32Const(0)
            .RefIsNull()
        .End()
      .End();

    const bin = builder.WebAssembly();
    bin.trim();

    assert.throws(() => new WebAssembly.Module(bin.get()), WebAssembly.CompileError, "WebAssembly.Module doesn't validate: ref.is_null to type I32 expected a reference type, in function at index 0 (evaluating 'new WebAssembly.Module(bin.get())')");
}

{
    const builder = (new Builder())
      .Type().End()
      .Import()
            .Table("imp", "tbl", {initial: 2, element: "funcref"})
      .End()
      .Function().End()
      .Export()
          .Function("j")
      .End()
      .Code()
        .Function("j", { params: [], ret: "void" })
            .I32Const(0)
            .I32Const(0)
            .TableSet(0)
        .End()
      .End();

    const bin = builder.WebAssembly();
    bin.trim();

    assert.throws(() => new WebAssembly.Module(bin.get()), WebAssembly.CompileError, "WebAssembly.Module doesn't validate: table.set value to type I32 expected Funcref, in function at index 0 (evaluating 'new WebAssembly.Module(bin.get())')");
}

{
    const builder = (new Builder())
      .Type().End()
      .Import()
            .Table("imp", "tbl", {initial: 2, element: "funcref"})
      .End()
      .Function().End()
      .Export()
          .Function("j")
      .End()
      .Code()
        .Function("j", { params: ["externref"], ret: "void" })
            .I32Const(0)
            .GetLocal(0)
            .TableSet(0)
        .End()
      .End();

    const bin = builder.WebAssembly();
    bin.trim();

    assert.throws(() => new WebAssembly.Module(bin.get()), WebAssembly.CompileError, "WebAssembly.Module doesn't validate: table.set value to type Externref expected Funcref, in function at index 0 (evaluating 'new WebAssembly.Module(bin.get())')");
}

{
    const builder = (new Builder())
      .Type().End()
      .Import()
            .Table("imp", "tbl", {initial: 2, element: "externref"})
      .End()
      .Function().End()
      .Export()
          .Function("j")
      .End()
      .Code()
        .Function("j", { params: [], ret: "funcref" })
            .I32Const(0)
            .TableGet(0)
        .End()
      .End();

    const bin = builder.WebAssembly();
    bin.trim();

    assert.throws(() => new WebAssembly.Module(bin.get()), WebAssembly.CompileError, "WebAssembly.Module doesn't validate: control flow returns with unexpected type. Externref is not a Funcref, in function at index 0 (evaluating 'new WebAssembly.Module(bin.get())')");
}

{
    assert.throws(() => new WebAssembly.Table({initial:2, element:"i32"}), TypeError, "WebAssembly.Table expects its 'element' field to be the string 'funcref' or 'externref'");
}
