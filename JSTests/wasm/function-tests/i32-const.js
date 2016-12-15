import * as assert from '../assert.js';
import Builder from '../Builder.js';


const builder = (new Builder())
      .Type().End()
      .Function().End()
      .Export()
          .Function("answer")
          .Function("minInt")
      .End()
      .Code()
          .Function("answer", { params: [], ret: "i32" })
              .I32Const(42)
          .End()

          .Function("minInt", { params: [], ret: "i32" })
             .I32Const(-1)
          .End()

          .Function("minInt", { params: [], ret: "i32" })
             .I32Const(0xffffffff)
          .End()
      .End();

const bin = builder.WebAssembly().get();
const module = new WebAssembly.Module(bin);
const instance = new WebAssembly.Instance(module);
assert.eq(instance.exports.answer(), 42);
assert.eq(instance.exports.minInt(), -1);
assert.eq(instance.exports.minInt(), -1);
