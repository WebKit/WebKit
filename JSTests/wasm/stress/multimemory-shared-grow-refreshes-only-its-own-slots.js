//@ requireOptions("--useWasmMultiMemory=1")

import * as assert from "../assert.js";
import { instantiate } from "../wabt-wrapper.js";

const wat = `
(module
  (import "env" "shared" (memory $shared 1 8 shared))
  (import "env" "plain" (memory $plain 1 8))
  (func (export "sizeShared") (result i32) (memory.size $shared))
  (func (export "sizePlain") (result i32) (memory.size $plain))
  (func (export "storePlain") (param $a i32) (i32.store $plain (local.get $a) (i32.const 0xaa)))
  (func (export "loadPlain") (param $a i32) (result i32) (i32.load $plain (local.get $a))))`;

const shared = new WebAssembly.Memory({ initial: 1, maximum: 8, shared: true });
const plain = new WebAssembly.Memory({ initial: 1, maximum: 8 });

const { exports } = await instantiate(wat, { env: { shared, plain } }, { multi_memory: true, threads: true });

const secondPage = 65536;

for (let i = 0; i < wasmTestLoopCount; ++i) {
    exports.sizeShared();
    exports.sizePlain();
}

assert.eq(shared.grow(1), 1);
assert.eq(exports.sizeShared(), 2);

assert.eq(exports.sizePlain(), 1);
assert.throws(() => exports.storePlain(secondPage), WebAssembly.RuntimeError, "Out of bounds memory access");

assert.eq(plain.grow(1), 1);
assert.eq(exports.sizePlain(), 2);
exports.storePlain(secondPage);
assert.eq(exports.loadPlain(secondPage), 0xaa);
assert.eq(exports.sizeShared(), 2);

assert.eq(shared.grow(0), 2);
assert.eq(exports.sizeShared(), 2);
assert.eq(exports.sizePlain(), 2);
assert.eq(exports.loadPlain(secondPage), 0xaa);

const otherPlain = new WebAssembly.Memory({ initial: 1, maximum: 8 });
const other = await instantiate(wat, { env: { shared, plain: otherPlain } }, { multi_memory: true, threads: true });
assert.eq(shared.grow(1), 2);
assert.eq(other.exports.sizeShared(), 3);
assert.eq(exports.sizeShared(), 3);
assert.eq(other.exports.sizePlain(), 1);
