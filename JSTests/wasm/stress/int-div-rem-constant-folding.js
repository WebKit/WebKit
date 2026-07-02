import * as assert from "../assert.js";
import { instantiate } from "../wabt-wrapper.js";

// Test constant folding of integer div/rem with INT_MIN / -1 edge cases.
// Operations other than signed division should not throw an overflow trap

let wat = `
(module
  (func (export "i32_div_s") (result i32)
    i32.const -2147483648
    i32.const -1
    i32.div_s
  )
  (func (export "i64_div_s") (result i64)
    i64.const -9223372036854775808
    i64.const -1
    i64.div_s
  )
  (func (export "i32_rem_s") (result i32)
    i32.const -2147483648
    i32.const -1
    i32.rem_s
  )
  (func (export "i64_rem_s") (result i64)
    i64.const -9223372036854775808
    i64.const -1
    i64.rem_s
  )
  (func (export "i32_div_u") (result i32)
    i32.const -2147483648
    i32.const -1
    i32.div_u
  )
  (func (export "i64_div_u") (result i64)
    i64.const -9223372036854775808
    i64.const -1
    i64.div_u
  )
  (func (export "i32_rem_u") (result i32)
    i32.const -2147483648
    i32.const -1
    i32.rem_u
  )
  (func (export "i64_rem_u") (result i64)
    i64.const -9223372036854775808
    i64.const -1
    i64.rem_u
  )
)
`;

async function test() {
    const instance = await instantiate(wat);

    for (let i = 0; i < wasmTestLoopCount; i++) {
        // div_s(INT_MIN, -1) must trap (integer overflow)
        assert.throws(() => instance.exports.i32_div_s(), WebAssembly.RuntimeError, "Integer overflow");
        assert.throws(() => instance.exports.i64_div_s(), WebAssembly.RuntimeError, "Integer overflow");

        // INT_MIN % -1 == 0 (no overflow trap for rem_s)
        assert.eq(instance.exports.i32_rem_s(), 0);
        assert.eq(instance.exports.i64_rem_s(), 0n);

        // Unsigned: 0x80000000 / 0xFFFFFFFF == 0
        assert.eq(instance.exports.i32_div_u(), 0);
        // Unsigned: 0x8000000000000000 / 0xFFFFFFFFFFFFFFFF == 0
        assert.eq(instance.exports.i64_div_u(), 0n);

        // Unsigned: 0x80000000 % 0xFFFFFFFF == 0x80000000 (returned as signed: -2147483648)
        assert.eq(instance.exports.i32_rem_u(), -2147483648);
        // Unsigned: 0x8000000000000000 % 0xFFFFFFFFFFFFFFFF == 0x8000000000000000
        assert.eq(instance.exports.i64_rem_u(), -9223372036854775808n);
    }
}

await assert.asyncTest(test());
