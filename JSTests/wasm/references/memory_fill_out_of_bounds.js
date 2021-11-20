import * as assert from '../assert.js';
import { instantiate } from "../wabt-wrapper.js";

async function test() {
  let wat = `
  (module
    (import "env" "memory" (memory $mem0 1 1))
    (func (export "fill_oob")
      (memory.fill (i32.const 0) (i32.const 42) (i32.const 65537))
    )
  )
  `;

  let memory = new WebAssembly.Memory({initial: 1, maximum: 1});
  const instance = await instantiate(wat, {env: {"memory": memory}}, {reference_types: true});

  assert.throws(() => { instance.exports.fill_oob() }, WebAssembly.RuntimeError, "Out of bounds memory access");
}

assert.asyncTest(test());
