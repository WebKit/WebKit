//@ runDefaultWasm("-m", "--useConcurrentJIT=0", "--jitPolicyScale=0", "--wasmOMGOptimizationLevel=0")
import * as assert from '../assert.js'

var wasm_code = read('exception-thrown-from-call.wasm', 'binary')
var wasm_module = new WebAssembly.Module(wasm_code);
var wasm_instance = new WebAssembly.Instance(wasm_module);
var test = wasm_instance.exports.test;

for (let i = 0; i < 10; ++i)
  assert.eq(test(), 2);
