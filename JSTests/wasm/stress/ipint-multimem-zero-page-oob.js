//@ requireOptions("--useWasmMultiMemory=1")

import * as assert from "../assert.js";
import { instantiate } from "../wabt-wrapper.js";

// Test that loads and stores to a 0-page non-zero memory correctly trap OOB.

let wat = `
(module
  (import "js" "mem0" (memory 1))
  (import "js" "mem1" (memory 0 1))

  (func (export "i32_load") (param i32) (result i32) (local.get 0) (i32.load 1))
  (func (export "i64_load") (param i32) (result i64) (local.get 0) (i64.load 1))
  (func (export "i32_load8u") (param i32) (result i32) (local.get 0) (i32.load8_u 1))
  (func (export "i32_store") (param i32) (local.get 0) (i32.const 0) (i32.store 1))
  (func (export "i64_store") (param i32) (local.get 0) (i64.const 0) (i64.store 1))
  (func (export "i32_store8") (param i32) (local.get 0) (i32.const 0) (i32.store8 1))
)
`

async function test() {
    const mem0 = new WebAssembly.Memory({ initial: 1 });
    const mem1 = new WebAssembly.Memory({ initial: 0, maximum: 1 });

    const instance = await instantiate(wat, { js: { mem0, mem1 } }, { multi_memory: true });

    const ops = ["i32_load", "i64_load", "i32_load8u", "i32_store", "i64_store", "i32_store8"];
    const addrs = [0, 1, 100, 65535];

    for (const op of ops) {
        for (const addr of addrs) {
            assert.throws(() => instance.exports[op](addr), WebAssembly.RuntimeError, "Out of bounds memory access");
        }
    }
}

await assert.asyncTest(test());
