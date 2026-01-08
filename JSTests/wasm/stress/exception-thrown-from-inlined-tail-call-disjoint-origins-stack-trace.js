//@ requireOptions("--jitPolicyScale=0.1", "--useConcurrentJIT=0", "--wasmInliningMaximumWasmCalleeSize=2147483647", "--wasmInliningBudget=1000000", "--wasmInliningMaximumDepth=10", "--wasmInliningMaximumCount=1000", "--wasmFunctionIndexRangeToCompile=8:100")
//@ skip unless $isOMGPlatform
import * as assert from '../assert.js'

var wasm_code = read('exception-thrown-from-inlined-tail-call-disjoint-origins-stack-trace.wasm', 'binary')
var wasm_module = new WebAssembly.Module(wasm_code);
var wasm_instance = new WebAssembly.Instance(wasm_module, { js: { jsThrow: (i) => { /*print('throwing error:', i);*/ throw new Error(i); } } });
let { test, inc_state } = wasm_instance.exports;

const msg = ["throw0@wasm-function[1];b@wasm-function[21];test@wasm-function[24]","throw1@wasm-function[2];do_tail_b@wasm-function[23];test@wasm-function[24]","throw2@wasm-function[3];test@wasm-function[24]","throw3@wasm-function[4];a@wasm-function[20];test@wasm-function[24]","throw4@wasm-function[5];do_tail_a@wasm-function[22];test@wasm-function[24]"]

try {
  for (let state = 0; state < 5; ++state) {
    for (let i = 0; i < wasmTestLoopCount; ++i) {
      // print("---: ", state)
      try {
        test();
        assert.fail("Expected an error");
      } catch (e) {
        // print("---: ", state, " done")
        assert.eq(e.message, state.toString())
        let stack = e.stack.split('\n')
        stack = stack.slice(1, stack.length - 1).join(';')
        assert.eq(stack, msg[state])
        // msg[state] = stack;
      }
    }
    inc_state();
  }

} catch (e) {
  print(`Unexpected exception thrown: ${e} ${e.stack}`);
  throw e
}

// print("success")
// print(JSON.stringify(msg))
