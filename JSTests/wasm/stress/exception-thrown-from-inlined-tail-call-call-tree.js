//@ requireOptions("--jitPolicyScale=0", "--useOMGInlining=1")
//@ skip unless $isOMGPlatform
import * as assert from '../assert.js'

var wasm_code = read('exception-thrown-from-inlined-tail-call-call-tree.wasm', 'binary')
var wasm_module = new WebAssembly.Module(wasm_code);
var wasm_instance = new WebAssembly.Instance(wasm_module,
  { m: { jsThrow: () => { throw new Error("jsThrows threw") } } });
var test = wasm_instance.exports.test;

for (let i = 0; i < 1000; ++i) {
  assert.eq(test(0, 0, 0), 1); // No tail call threw
  assert.eq(test(1, 0, 0), 2); // $tagA was thrown
  assert.eq(test(0, 1, 0), 3); // $tagB was thrown
  assert.eq(test(1, 0, 1), 2); // $tagA was thrown first
  assert.throws(()=>test(0, 0, 1), Error, "jsThrows threw");
}
