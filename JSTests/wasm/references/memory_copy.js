import * as assert from '../assert.js';
import { instantiate } from "../wabt-wrapper.js";

let wat = `
(module
  (memory $mem0 1)
  (data (i32.const 0) "wasm")
  (func (export "run") (result i32)
    (memory.copy (i32.const 3) (i32.const 0) (i32.const 1))
    (i32.load8_u (i32.const 3))
  )
)
`;
async function test() {
  const instance = await instantiate(wat, {}, {reference_types: true});
  assert.eq(instance.exports.run(), 119 /* 'w' ascii code */);
}

assert.asyncTest(test());
