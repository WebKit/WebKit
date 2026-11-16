//@ requireOptions("--useWasmMultiMemory=1")

import * as assert from "../assert.js";
import { instantiate } from "../wabt-wrapper.js";

// test bulk linear memory instructions with memories other than 0

let wat = `
(module
  (import "js" "memory0" (memory 1))
  (import "js" "memory1" (memory 1))

  (func (export "memory_size") (result i32) (memory.size 1))
  (func (export "memory_grow") (param i32) (result i32) (local.get 0) (memory.grow 1))
  (func (export "memory_fill") (param i32 i32 i32)
    ;; (i, value, n) fill n bytes starting at address i
    local.get 0
    local.get 1
    local.get 2
    memory.fill 1
  )
  (func (export "memory_copy") (param i32 i32 i32) ;; (i1, i2, n)
    ;; copy n bytes starting at index i1 of source into region starting at index i2 of destination
    local.get 0
    local.get 1
    local.get 2
    memory.copy 1 0 ;; destination comes first
  )
  (func (export "memory_init") (param i32 i32 i32) ;; (i, j, n)
    local.get 0
    local.get 1
    local.get 2
    memory.init 0 0 ;; memory comes first, data segment second
    local.get 0
    local.get 1
    local.get 2
    memory.init 1 1
  )

  (func (export "i64_load") (param i32) (result i64) (local.get 0) (i64.load 1))

  (data "ABCDEFGH")
  (data "IJKLMNOP")
)
`

// memory.{size,grow,fill,copy,init}

async function test() {
    const mem0 = new WebAssembly.Memory({ initial: 1 });
    const mem1 = new WebAssembly.Memory({ initial: 1 });

    const instance = await instantiate(wat, { js: { memory0: mem0, memory1: mem1 } }, { multi_memory: true });

    assert.eq(instance.exports.i64_load(0), 0n); // data segments should be passive, not active (actives automatically load at initialization time)
    assert.eq(instance.exports.memory_size(), 1);
    assert.eq(instance.exports.memory_grow(2), 1); // memory.grow returns old size

    for(let i = 0; i < wasmTestLoopCount; i++) {
        assert.eq(instance.exports.memory_size(), 3);
        assert.eq(instance.exports.i64_load(0), 0n);
        instance.exports.memory_init(0, 0, 8);
        assert.eq(instance.exports.i64_load(0), 0x504F4E4D4C4B4A49n);
        instance.exports.memory_fill(0, 0x02, 100);
        assert.eq(instance.exports.i64_load(0), 0x0202020202020202n);
        instance.exports.memory_copy(1, 0, 4);
        assert.eq(instance.exports.i64_load(0), 0x0202024443424102n);

        instance.exports.memory_fill(0, 0x00, 100); // clear memory before next iteration
    }
}

await assert.asyncTest(test())
