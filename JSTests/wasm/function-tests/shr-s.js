import * as assert from '../assert.js';
import Builder from '../Builder.js';

const builder = (new Builder())
      .Type().End()
      .Function().End()
      .Export()
          .Function("i32ShrS")
          .Function("i64ShrS")
      .End()
      .Code()
          .Function("i32ShrS", { params: ["i32", "i32"], ret: "i32" })
              .GetLocal(0)
              .GetLocal(1)
              .I32ShrS()
          .End()

          .Function("i64ShrS", { params: ["i64", "i64"], ret: "i64" })
              .GetLocal(0)
              .GetLocal(1)
              .I64ShrS()
          .End()

      .End();

const bin = builder.WebAssembly().get();
const module = new WebAssembly.Module(bin);
const instance = new WebAssembly.Instance(module);
assert.eqI32(instance.exports.i32ShrS(1, 1), 0);
assert.eqI32(instance.exports.i32ShrS(1, 2), 0);
assert.eqI32(instance.exports.i32ShrS(0xf, 2), 0x3);
assert.eqI32(instance.exports.i32ShrS(0xf0000000, 1), 0xf8000000);
