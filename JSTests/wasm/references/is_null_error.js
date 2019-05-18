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

    assert.throws(() => new WebAssembly.Module(bin.get()), WebAssembly.CompileError, "WebAssembly.Module doesn't validate: ref.is_null to type I32 expected Anyref, in function at index 0 (evaluating 'new WebAssembly.Module(bin.get())')");
}
