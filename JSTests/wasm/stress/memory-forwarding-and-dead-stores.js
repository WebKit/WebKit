import { compile, instantiate } from "../wabt-wrapper.js";
import * as assert from "../assert.js";

let memory;
let observed;
const imports = {
    e: {
        observe(p) { observed = new Int32Array(memory.buffer)[p >> 2]; },
        overwrite(p) { new Int32Array(memory.buffer)[p >> 2] += 100; },
    },
};

const instance = await instantiate(`
(module
  (import "e" "observe" (func $observe (param i32)))
  (import "e" "overwrite" (func $overwrite (param i32)))
  (memory (export "memory") 1)

  (func (export "loadAcrossCall") (param $p i32) (result i32)
    (i32.load (local.get $p))
    (call $overwrite (local.get $p))
    (i32.load (local.get $p))
    i32.sub)

  (func (export "storesAroundCall") (param $p i32)
    (i32.store (local.get $p) (i32.const 1))
    (call $observe (local.get $p))
    (i32.store (local.get $p) (i32.const 2)))

  (func (export "storesAroundTrap") (param $p i32) (param $q i32)
    (i32.store (local.get $p) (i32.const 1))
    (i32.store (local.get $q) (i32.const 3))
    (i32.store (local.get $p) (i32.const 2))))
`, imports);
memory = instance.exports.memory;
const words = new Int32Array(memory.buffer);
const { loadAcrossCall, storesAroundCall, storesAroundTrap } = instance.exports;

for (let i = 0; i < 1e4; ++i) {
    words[0] = i;
    assert.eq(loadAcrossCall(0), -100);

    observed = undefined;
    storesAroundCall(8);
    assert.eq(observed, 1);
    assert.eq(words[2], 2);

    words[4] = 0;
    storesAroundTrap(16, 32);
    assert.eq(words[4], 2);
    assert.eq(words[8], 3);

    words[4] = 0;
    assert.throws(() => storesAroundTrap(16, 65536), WebAssembly.RuntimeError, "Out of bounds memory access");
    assert.eq(words[4], 1);
}

// A fresh instance per iteration lets every call grow its memory, while the module's code tiers up.
const growModule = await compile(`
(module
  (memory (export "memory") 1 2)
  (func (export "lastWordAcrossGrow") (result i32)
    (i32.store (i32.sub (i32.shl (memory.size) (i32.const 16)) (i32.const 4)) (i32.const 5))
    (drop (memory.grow (i32.const 1)))
    (i32.load (i32.sub (i32.shl (memory.size) (i32.const 16)) (i32.const 4)))))
`);
for (let i = 0; i < 2000; ++i) {
    const { lastWordAcrossGrow } = new WebAssembly.Instance(growModule).exports;
    assert.eq(lastWordAcrossGrow(), 0);
    assert.eq(lastWordAcrossGrow(), 5);
}

const atomics = (await instantiate(`
(module
  (memory 1)
  (func (export "loadAcrossRmw") (param $p i32) (result i32)
    (i32.load (local.get $p))
    (drop (i32.atomic.rmw.add (local.get $p) (i32.const 5)))
    (i32.load (local.get $p))
    i32.sub)

  (func (export "storeAcrossCmpxchg") (param $p i32) (result i32)
    (i32.store (local.get $p) (i32.const 10))
    (drop (i32.atomic.rmw.cmpxchg (local.get $p) (i32.const 10) (i32.const 20)))
    (i32.load (local.get $p)))

  (func (export "storesAroundFence") (param $p i32) (result i32)
    (i32.store (local.get $p) (i32.const 1))
    (atomic.fence)
    (i32.atomic.load (local.get $p))
    (i32.store (local.get $p) (i32.const 2))
    (i32.load (local.get $p))
    i32.add))
`, {}, { threads: true })).exports;

for (let i = 0; i < 1e4; ++i) {
    assert.eq(atomics.loadAcrossRmw(0), -5);
    assert.eq(atomics.storeAcrossCmpxchg(8), 20);
    assert.eq(atomics.storesAroundFence(16), 3);
}
