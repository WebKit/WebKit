import * as assert from '../assert.js';
import { instantiate } from "../wabt-wrapper.js";

async function test() {
    await instantiate(`
        (module
          (func $foo (result i32 i32) unreachable)
          (func (param i32) (result i32 i32 i32)
                (local.get 0)
                (loop (param i32) (result i32 i32 i32)
                      call $foo)
                )
          )
    `);
}

assert.asyncTest(test());
