//@ skip if $architecture == "arm"
import * as assert from '../assert.js';
import { instantiate } from "../wabt-wrapper.js";

let wat = `
(module
  (import "env" "memory" (memory $mem0 1 7 shared))
  (func (export "run_copy")
    (memory.copy (i32.const 12) (i32.const 0) (i32.const 12))
  )
  (func (export "get") (param $address i32) (result i32)
    (i32.load (local.get $address))
  )
)
`;
async function test() {
  let memory = new WebAssembly.Memory({initial: 1, maximum: 7, shared: true});
  let memoryView = new Uint32Array(memory.buffer);
  memoryView[0] = 37;
  memoryView[1] = 42;
  memoryView[2] = 73;

  const instance = await instantiate(wat, {env: {"memory": memory}}, {reference_types: true});
  const sizeOfUint32InBytes = 4;
  assert.eq(instance.exports.get(0 * sizeOfUint32InBytes), 37);
  assert.eq(instance.exports.get(1 * sizeOfUint32InBytes), 42);
  assert.eq(instance.exports.get(2 * sizeOfUint32InBytes), 73);

  instance.exports.run_copy();

  assert.eq(instance.exports.get(3 * sizeOfUint32InBytes), 37);
  assert.eq(instance.exports.get(4 * sizeOfUint32InBytes), 42);
  assert.eq(instance.exports.get(5 * sizeOfUint32InBytes), 73);
}

assert.asyncTest(test());
