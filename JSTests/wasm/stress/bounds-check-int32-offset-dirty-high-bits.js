//@ requireOptions("--useWasmFastMemory=0")
import { instantiate } from "../wabt-wrapper.js";
import * as assert from "../assert.js";

async function test() {
    const instance = await instantiate(`
        (module
          (memory 1)
          (func $load (param i32) (result i32)
            (i32.load offset=4 (local.get 0)))
          (func (export "run") (param i64) (param i32) (result i32)
            (local $sum i32)
            (loop $top
              (local.set $sum
                (i32.add (local.get $sum)
                  (call $load (i32.wrap_i64 (local.get 0)))))
              (local.set 1 (i32.sub (local.get 1) (i32.const 1)))
              (br_if $top (local.get 1)))
            (local.get $sum)))
    `, {});

    const dirty = 0xffffffff00000000n;
    for (let i = 0; i < wasmTestLoopCount; ++i)
        assert.eq(instance.exports.run(dirty, 100), 0);
}

await assert.asyncTest(test());
