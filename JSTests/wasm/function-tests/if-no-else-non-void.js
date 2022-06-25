import * as assert from '../assert.js';
import Builder from '../Builder.js';

const builder = (new Builder())
      .Type().End()
      .Function().End()
      .Code()
          .Function("bad-if", { params: [], ret: "i32" })
              .I32Const(0)
              .If("i32", b => b.I32Const(0))
          .End()

      .End();

const bin = builder.WebAssembly().get();
assert.throws(() => new WebAssembly.Module(bin), WebAssembly.CompileError, "WebAssembly.Module doesn't validate:  block with type: () -> [I32] returns: 1 but stack has: 0 values, in function at index 0 (evaluating 'new WebAssembly.Module(bin)')");
