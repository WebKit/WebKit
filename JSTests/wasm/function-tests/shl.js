import * as assert from '../assert.js';
import Builder from '../Builder.js';

const builder = (new Builder())
      .Type().End()
      .Function().End()
      .Export()
          .Function("i32Shl")
          .Function("i64Shl")
      .End()
      .Code()
          .Function("i32Shl", { params: ["i32", "i32"], ret: "i32" })
              .GetLocal(0)
              .GetLocal(1)
              .I32Shl()
          .End()

          .Function("i64Shl", { params: ["i64", "i64"], ret: "i64" })
              .GetLocal(0)
              .GetLocal(1)
              .I64Shl()
          .End()

      .End();

const bin = builder.WebAssembly().get();
const module = new WebAssembly.Module(bin);
const instance = new WebAssembly.Instance(module);
assert.eqI32(instance.exports.i32Shl(1, 1), 2);
assert.eqI32(instance.exports.i32Shl(1, 2), 4);
assert.eqI32(instance.exports.i32Shl(0xf, 2), 0x3c);
assert.eqI32(instance.exports.i32Shl(0xf0000000, 1), 0xe0000000);
