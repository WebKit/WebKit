import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

function bytes(memory, offset, length) {
    return Array.from(new Uint8Array(memory.buffer, offset, length));
}

async function test() {
    const instance = await instantiate(`
      (module
        (memory (export "memory") 1)
        (data "abcdefgh")
        (func (export "init") (param i32 i32 i32)
          (memory.init 0 (local.get 0) (local.get 1) (local.get 2)))
        (func (export "drop")
          (data.drop 0)))
    `);
    const { memory, init, drop } = instance.exports;

    for (let i = 0; i < wasmTestLoopCount; i++) {
        init(4, 0, 8);
        assert.eq(bytes(memory, 4, 8), [97, 98, 99, 100, 101, 102, 103, 104]);
        init(20, 2, 3);
        assert.eq(bytes(memory, 20, 3), [99, 100, 101]);
        init(65536, 0, 0);
        assert.throws(() => init(65537, 0, 0), WebAssembly.RuntimeError, "Out of bounds memory access");
        assert.throws(() => init(65535, 0, 2), WebAssembly.RuntimeError, "Out of bounds memory access");
        assert.throws(() => init(0, 8, 1), WebAssembly.RuntimeError, "Out of bounds memory access");
        assert.throws(() => init(0, 0, 9), WebAssembly.RuntimeError, "Out of bounds memory access");
        assert.throws(() => init(0, 0xffffffff, 1), WebAssembly.RuntimeError, "Out of bounds memory access");
    }

    drop();
    init(0, 0, 0);
    assert.throws(() => init(0, 0, 1), WebAssembly.RuntimeError, "Out of bounds memory access");
    assert.throws(() => init(0, 1, 0), WebAssembly.RuntimeError, "Out of bounds memory access");

    const active = await instantiate(`
      (module
        (memory (export "memory") 1)
        (data (i32.const 0) "xy")
        (func (export "init") (param i32 i32 i32)
          (memory.init 0 (local.get 0) (local.get 1) (local.get 2))))
    `);
    for (let i = 0; i < wasmTestLoopCount; i++) {
        active.exports.init(0, 0, 0);
        assert.throws(() => active.exports.init(0, 0, 1), WebAssembly.RuntimeError, "Out of bounds memory access");
    }
}

await assert.asyncTest(test());

