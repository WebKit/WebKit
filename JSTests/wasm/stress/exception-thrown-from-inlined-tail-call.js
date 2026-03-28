//@ requireOptions("--jitPolicyScale=0", "--useOMGInlining=1")
//@ skip unless $isOMGPlatform
import * as assert from '../assert.js'

var wasm_code = read('exception-thrown-from-inlined-tail-call.wasm', 'binary')
var wasm_module = new WebAssembly.Module(wasm_code);
var wasm_instance = new WebAssembly.Instance(wasm_module);
var test = wasm_instance.exports.test;

for (let i = 0; i < 1000; ++i) {
  assert.eq(test(), 2);
}
