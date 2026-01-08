//@ requireOptions("--jitPolicyScale=0.1", "--useConcurrentJIT=0", "--wasmInliningMaximumWasmCalleeSize=2147483647", "--wasmInliningBudget=1000000", "--wasmInliningMaximumDepth=10", "--wasmInliningMaximumCount=1000", "--wasmFunctionIndexRangeToCompile=8:100")
//@ skip unless $isOMGPlatform
import * as assert from '../assert.js'

var wasm_code = read('exception-thrown-from-inlined-tail-call-disjoint-origins-indirect-try-table.wasm', 'binary')
var wasm_module = new WebAssembly.Module(wasm_code);
var wasm_instance = new WebAssembly.Instance(wasm_module);
let { test, inc_state } = wasm_instance.exports;

try {
  for (let state = 0; state < 5; ++state) {
    for (let i = 0; i < wasmTestLoopCount; ++i) {
      // print("---: ", state)
      let res = test();
      // print("---: ", state, " done")
      assert.eq(res, state);
    }
    inc_state();
  }

} catch (e) {
  print(`Unexpected exception thrown: ${e} ${e.stack}`);
  throw e
}