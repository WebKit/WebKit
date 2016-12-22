import * as assert from '../assert.js';
import Builder from '../Builder.js';

const builder = (new Builder())
      .Type().End()
      .Function().End()
      .Export()
          .Function("br1Default")
          .Function("br0Default")
          .Function("br1Case")
          .Function("br0Case")
      .End()
      .Code()
          .Function("br1Default", { params: [], ret: "i32" })
              .Block("void", b => {
                  return b.I32Const(0)
                  .I32Const(100)
                  .BrTable(1, 1)
              })
             .Unreachable()
          .End()

          .Function("br0Default", { params: [], ret: "i32" })
              .I32Const(0)
              .I32Const(100)
              .BrTable(0, 0)
          .End()

          .Function("br1Case", { params: [], ret: "i32" })
              .Block("void", b => {
                  return b.I32Const(0)
                  .I32Const(100)
                  .BrTable(1, 1)
              })
             .Unreachable()
          .End()

          .Function("br0Case", { params: [], ret: "i32" })
              .I32Const(0)
              .I32Const(0)
              .BrTable(0, 0)
          .End()

      .End();

const bin = builder.WebAssembly().get();
const module = new WebAssembly.Module(bin);
const instance = new WebAssembly.Instance(module);
assert.eq(instance.exports.br1Default(), 0)
assert.eq(instance.exports.br0Default(), 0)
assert.eq(instance.exports.br1Case(), 0)
assert.eq(instance.exports.br0Case(), 0)
