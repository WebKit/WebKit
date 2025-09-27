//@ skip
// WORK IN PROGRESS - requires us to rebuild a usable VM entry frame above the implanted
// slice for unwinding to find the VM boundary as expected. Currently crashes.

// Here we test the throwing of Wasm exceptions in JS code called from Wasm code via a
// Suspending wrapper. An exception like that turns into a rejection of the promise
// returned to Suspending. The rejection (being a Wasm exceptions) should then turn back
// into an exception propagated through the suspended Wasm stack. If uncaught in Wasm, an
// exception should turn back into a rejection of the promise returned by promising().

import Builder from '../Builder.js'
import * as assert from '../assert.js'

let exceptionTag = null;
let shouldNotBeCalledFlag = false;

async function throwingFunc() {
    // Throw a Wasm exception with payload value 42
    throw new WebAssembly.Exception(exceptionTag, [42]);
}

function shouldNotBeCalled() {
    shouldNotBeCalledFlag = true;
}

async function testCatchWasmExceptionThrownFromJS() {

  const b = new Builder();
  b.Type().End()
      .Import()
          .Function("env", "throwingFunc", { params: [], ret: "i32" })
          .Function("env", "shouldNotBeCalled", { params: [], ret: "void" })
      .End()
      .Function().End()
      .Exception().Signature({ params: ["i32"] }).End()
      .Export()
          .Function("testCatch")
          .Exception("tag", 0)
      .End()
      .Code()
          .Function("testCatch", { params: [], ret: "i32" })
              .Try("i32")
                  .Call(0) // call throwingFunc
              .Catch(0)
                  // Exception caught, payload is on stack
              .End()
          .End()
      .End();

  const bin = b.WebAssembly().get();
  const module = new WebAssembly.Module(bin);
  const instance = new WebAssembly.Instance(module, {
    env: {
      throwingFunc: new WebAssembly.Suspending(throwingFunc),
      shouldNotBeCalled: shouldNotBeCalled,
    }
  });

  exceptionTag = instance.exports.tag;
  const promisingFunc = WebAssembly.promising(instance.exports.testCatch);
  try {
    const result = await promisingFunc();
    assert.eq(result, 42, "Should catch exception and return payload value");
  } catch(error) {
    throw new Error("Exception has not been caught in Wasm code");
  }
}

async function testPropagateUncaughtWasmException() {
  shouldNotBeCalledFlag = false;

  exceptionTag = new WebAssembly.Tag({ parameters: ["i32"] });

  const b = new Builder();
  b.Type().End()
      .Import()
          .Function("env", "throwingFunc", { params: [], ret: "i32" })
          .Function("env", "shouldNotBeCalled", { params: [], ret: "void" })
      .End()
      .Function().End()
      .Export()
          .Function("testNoCatch")
      .End()
      .Code()
          .Function("testNoCatch", { params: [], ret: "i32" })
              .Call(0) // call throwingFunc
              .Drop()
              .Call(1) // call shouldNotBeCalled
              .I32Const(999)
          .End()
      .End();

  const bin = b.WebAssembly().get();
  const module = new WebAssembly.Module(bin);
  const instance = new WebAssembly.Instance(module, {
    env: {
      throwingFunc: new WebAssembly.Suspending(throwingFunc),
      shouldNotBeCalled: shouldNotBeCalled
    }
  });

  const promisingFunc = WebAssembly.promising(instance.exports.testNoCatch);

  try {
    await promisingFunc();
    throw new Error("Exception should have propagated out");
  } catch (error) {
    assert.truthy(error instanceof WebAssembly.Exception, "Should be a WebAssembly.Exception");
    assert.eq(error.getArg(exceptionTag, 0), 42, "Exception payload should match");
  }

  assert.falsy(shouldNotBeCalledFlag, "Wasm function should not have continued after exception");
}

await testCatchWasmExceptionThrownFromJS();
await testPropagateUncaughtWasmException();
