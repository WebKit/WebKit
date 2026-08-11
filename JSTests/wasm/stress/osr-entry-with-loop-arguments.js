import * as assert from '../assert.js';
import { instantiate } from "../wabt-wrapper.js";

let wat = `
(module
  (func (export "test") (param $countArg i32) (result i32) (local $result i32)
    i32.const 0
    (loop (param i32) (result i32)
      i32.const 1
      i32.add
      local.tee $result
      local.get $result
      local.get $countArg
      i32.lt_u
      br_if 0
    )
  )
)
`;

async function test() {
    let instance = await instantiate(wat);

    let result = instance.exports.test(wasmTestLoopCount);
    if (result !== wasmTestLoopCount)
        throw new Error(`Expected ${wasmTestLoopCount}, but got: ${result}`);
}

await assert.asyncTest(test());
