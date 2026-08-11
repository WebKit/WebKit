import * as assert from '../assert.js';
import Builder from '../Builder.js';

const builder = (new Builder())
      .Type().End()
      .Function().End()
      .Export()
          .Function("dead-call")
      .End()
      .Code()
          .Function("crash", { params: [], ret: "i32" })
              .Unreachable()
          .End()

          .Function("dead-call", { params: [], ret: "i32" })
              .I32Const(0)
              .If("i32", b =>
                  b.Call(0)
                  .Return()
                  .Else()
                  .I32Const(1)
                 )
          .End()

      .End();

const bin = builder.WebAssembly().get();
const module = new WebAssembly.Module(bin);
const instance = new WebAssembly.Instance(module);
