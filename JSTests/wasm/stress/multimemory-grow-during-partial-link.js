//@ requireOptions("--useWasmMultiMemory=1")

import * as assert from "../assert.js";
import { compile } from "../wabt-wrapper.js";

// A grow reaching an instance whose memory imports were not all resolved (LinkError midway,
// or a re-entrant grow from an import getter) must not touch the still-empty memory slots.

const wat = `
(module
  (import "env" "m0" (memory $m0 1 8))
  (import "env" "m1" (memory $m1 1 8))
  (func (export "size0") (result i32) (memory.size $m0))
  (func (export "load0") (param $a i32) (result i32) (i32.load $m0 (local.get $a)))
  (func (export "load1") (param $a i32) (result i32) (i32.load $m1 (local.get $a))))`;
const module = await compile(wat, { multi_memory: true });

// 1. The second memory import fails, so linking throws after the first memory was set.
{
    const mem0 = new WebAssembly.Memory({ initial: 1, maximum: 8 });
    assert.throws(() => new WebAssembly.Instance(module, { env: { m0: mem0, m1: 42 } }), WebAssembly.LinkError, "Memory import env:m1 is not an instance of WebAssembly.Memory");
    assert.eq(mem0.grow(0), 1);
    assert.eq(mem0.grow(1), 1);
    assert.eq(mem0.buffer.byteLength, 2 * 65536);
}

// 2. The getter of the second memory import grows the already-linked first memory.
{
    const mem0 = new WebAssembly.Memory({ initial: 1, maximum: 8 });
    const mem1 = new WebAssembly.Memory({ initial: 1, maximum: 8 });
    const importObject = { env: { m0: mem0, get m1() { assert.eq(mem0.grow(1), 1); return mem1; } } };
    const instance = new WebAssembly.Instance(module, importObject);
    assert.eq(instance.exports.size0(), 2);
    assert.eq(instance.exports.load0(65536), 0);
    assert.eq(instance.exports.load1(0), 0);
}
