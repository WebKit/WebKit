import * as assert from '../assert.js';
import Builder from '../Builder.js';

const builder = (new Builder())
      .Type().End()
      .Function().End()
      .Export()
          .Function("foo")
      .End()
      .Code()
          .Function("foo", { params: [], ret: "i32" })
              .I32Const(1)
          .End()
      .End();

const bin = builder.WebAssembly().get();
const module = new WebAssembly.Module(bin);
const instance = new WebAssembly.Instance(module);

assert.eq(Object.getPrototypeOf(instance.exports.foo), Function.prototype);
{
    assert.truthy(typeof instance.exports.foo === "function", "is_function bytecode should handle wasm function.");
    let value = typeof instance.exports.foo;
    assert.eq(value, "function", "the result of typeof should be 'function'");
}
