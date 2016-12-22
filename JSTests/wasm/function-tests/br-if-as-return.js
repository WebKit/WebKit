import * as assert from '../assert.js';
import Builder from '../Builder.js';

const builder = (new Builder())
      .Type().End()
      .Function().End()
      .Export()
          .Function("br1")
          .Function("br0")
      .End()
      .Code()
          .Function("br1", { params: [], ret: "i32" })
              .Block("void", b => {
                  return b.I32Const(0)
                  .I32Const(1)
                  .BrIf(1)
              })
             .Unreachable()
          .End()

          .Function("br0", { params: [], ret: "i32" })
              .I32Const(0)
              .I32Const(1)
              .BrIf(0)
          .End()

      .End();

const bin = builder.WebAssembly().get();
const module = new WebAssembly.Module(bin);
const instance = new WebAssembly.Instance(module);
assert.eq(instance.exports.br1(), 0)
assert.eq(instance.exports.br0(), 0)
