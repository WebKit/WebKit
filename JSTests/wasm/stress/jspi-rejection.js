//@ requireOptions("--useJSPI=1")

import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
  (import "env" "createPromise" (func $createPromise (result i32)))
  (import "env" "shouldNotBeCalled" (func $shouldNotBeCalled))
  (func $testFunc (export "testFunc") (result i32)
    call $createPromise
    call $shouldNotBeCalled
  )
)`;

let doRejectPromise = null;
let hasBeenCalledFlag = false;
const rejectionReason = new Error("Test rejection reason");

function createPromise() {
    const promise = new Promise((resolve, reject) => {
        doRejectPromise = reject;
    });
    return promise;
}

async function rejectByThrowing() {
  throw rejectionReason;
}

function shouldNotBeCalled() {
    hasBeenCalledFlag = true;
}

async function test1() {
  hasBeenCalledFlag = false;

  const instance = await instantiate(wat, {
    env: {
      createPromise: new WebAssembly.Suspending(createPromise),
      shouldNotBeCalled: shouldNotBeCalled
    }
  });

  const promisingFunc = WebAssembly.promising(instance.exports.testFunc);
  const resultPromise = promisingFunc();
  doRejectPromise(rejectionReason);

  try {
    await resultPromise;
    throw new Error("test1: Promise should have been rejected");
  } catch (error) {
    assert.eq(error, rejectionReason, "test1: Rejection reason should match");
  }

  assert.falsy(hasBeenCalledFlag, "test1: WASM function has been resumed after rejection");
}

async function test2() {
  hasBeenCalledFlag = false;

  const instance = await instantiate(wat, {
    env: {
      createPromise: new WebAssembly.Suspending(rejectByThrowing),
      shouldNotBeCalled: shouldNotBeCalled
    }
  });

  const promisingFunc = WebAssembly.promising(instance.exports.testFunc);
  const resultPromise = promisingFunc();

  try {
    await resultPromise;
    throw new Error("test2: Promise should have been rejected by throwing");
  } catch (error) {
    assert.eq(error, rejectionReason, "test2: Rejection reason should match");
  }

  assert.falsy(hasBeenCalledFlag, "test2: WASM function has been resumed after rejection");
}

await test1();
await test2();
