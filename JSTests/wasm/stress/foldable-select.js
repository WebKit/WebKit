import { instantiate } from "../wabt-wrapper.js";
import * as assert from "../assert.js";

let wat = `
(module
    (func (export "testZero") (result i32)
        i32.const 1
        i32.const 2
        i32.const 0
        select
        global.get $x
        i32.trunc_f32_s
        i32.add
    )
    (func (export "testNonzero") (result i32)
        i32.const 1
        i32.const 2
        i32.const 1
        select
        global.get $x
        i32.trunc_f32_s
        i32.add
    )
    (global $x f32 f32.const 42.0)
)
`;

async function test() {
  const instance = await instantiate(wat, {}, {});
  const { testZero, testNonzero } = instance.exports;
  assert.eq(testZero(), 44);
  assert.eq(testNonzero(), 43);
}

await assert.asyncTest(test());
