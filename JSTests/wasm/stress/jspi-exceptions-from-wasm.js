//@ requireOptions("--useJSPI=1")

// Here we test the throwing of a Wasm exception in Wasm code called from JS via a
// promising() wrapper. The exception should turn into a rejection of the promise
// returned by promising().

import Builder from '../Builder.js'
import * as assert from '../assert.js'

async function testWasmExceptionBecomesPromisingRejection() {
  const b = new Builder();
  b.Type().End()
      .Import().End()
      .Function().End()
      .Exception().Signature({ params: ["i32"] }).End()
      .Export()
          .Function("throwingWasmFunc")
          .Exception("tag", 0)
      .End()
      .Code()
          .Function("throwingWasmFunc", { params: [], ret: "i32" })
              .I32Const(42)  // Exception payload
              .Throw(0)      // Throw exception with tag 0
          .End()
      .End();

  const bin = b.WebAssembly().get();
  const module = new WebAssembly.Module(bin);
  const instance = new WebAssembly.Instance(module, { });

  const exceptionTag = instance.exports.tag;
  const promisingFunc = WebAssembly.promising(instance.exports.throwingWasmFunc);

  try {
    await promisingFunc();
    throw new Error("Promise should have been rejected");
  } catch (error) {
    assert.truthy(error instanceof WebAssembly.Exception, "Should be a WebAssembly.Exception");
    assert.eq(error.getArg(exceptionTag, 0), 42, "Exception payload should be 42");
  }
}

await testWasmExceptionBecomesPromisingRejection();
