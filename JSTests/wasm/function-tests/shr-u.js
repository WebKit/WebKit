import * as assert from '../assert.js';
import Builder from '../Builder.js';

const builder = (new Builder())
      .Type().End()
      .Function().End()
      .Export()
          .Function("i32ShrU")
          .Function("i64ShrU")
      .End()
      .Code()
          .Function("i32ShrU", { params: ["i32", "i32"], ret: "i32" })
              .GetLocal(0)
              .GetLocal(1)
              .I32ShrU()
          .End()

          .Function("i64ShrU", { params: ["i64", "i64"], ret: "i64" })
              .GetLocal(0)
              .GetLocal(1)
              .I64ShrU()
          .End()

      .End();

const bin = builder.WebAssembly().get();
const module = new WebAssembly.Module(bin);
const instance = new WebAssembly.Instance(module);
assert.eqI32(instance.exports.i32ShrU(1, 1), 0);
assert.eqI32(instance.exports.i32ShrU(1, 2), 0);
assert.eqI32(instance.exports.i32ShrU(0xf, 2), 0x3);
assert.eqI32(instance.exports.i32ShrU(0xf0000000, 1), 0x78000000);
