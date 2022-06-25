import * as assert from '../assert.js';
import Builder from '../Builder.js';

const builder = (new Builder())
      .Type().End()
      .Function().End()
      .Export()
          .Function("i32Rotr")
          .Function("i64Rotr")
      .End()
      .Code()
          .Function("i32Rotr", { params: ["i32", "i32"], ret: "i32" })
              .GetLocal(0)
              .GetLocal(1)
              .I32Rotr()
          .End()

          .Function("i64Rotr", { params: ["i64", "i64"], ret: "i64" })
              .GetLocal(0)
              .GetLocal(1)
              .I64Rotr()
          .End()

      .End();

const bin = builder.WebAssembly().get();
const module = new WebAssembly.Module(bin);
const instance = new WebAssembly.Instance(module);
assert.eqI32(instance.exports.i32Rotr(1, 1), 0x80000000);
assert.eqI32(instance.exports.i32Rotr(1, 2), 0x40000000);
assert.eqI32(instance.exports.i32Rotr(0xf, 2), 0xc0000003);
