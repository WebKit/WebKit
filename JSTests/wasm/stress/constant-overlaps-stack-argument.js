import { instantiate } from "../wabt-wrapper.js";
import * as assert from "../assert.js";

let wat = `
(module
    (func $0 (result i64 i64 i64 i64 i64 i64 i64 i64 i64)
        i64.const 1
        i64.const 2
        i64.const 3
        i64.const 4
        i64.const 5
        i64.const 6
        i64.const 7
        i64.const 8
        i64.const 9
    )
    (func (export "test") (result i64)
        call $0
        drop
        i64.const 42
        block
            nop
        end
        return
    )
)
`;

async function test() {
  const instance = await instantiate(wat, {}, { simd: true });
  const { test } = instance.exports;
  assert.eq(test(), 42n);
}

await assert.asyncTest(test());
