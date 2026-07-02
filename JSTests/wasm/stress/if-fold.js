import { instantiate } from "../wabt-wrapper.js";
import * as assert from "../assert.js";

let wat = `
(module
    (func (export "test32")
        (loop $loop
            i32.const 1000
            i32.const 1000
            i32.ne
            br_if $loop)
    )

    (func (export "test64")
        (loop $loop
            i64.const 1000
            i64.const 1000
            i64.ne
            br_if $loop)
    )
)
`;

async function test() {
  const instance = await instantiate(wat, {}, {});
  const { test32, test64 } = instance.exports;
  assert.eq(test32(), undefined);
  assert.eq(test64(), undefined);
}

await assert.asyncTest(test());
