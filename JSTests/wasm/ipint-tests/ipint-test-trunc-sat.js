import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (func (export "i32_ts_f32_s") (param f32) (result i32)
        (local.get 0)
        (i32.trunc_sat_f32_s)
    )
    (func (export "i32_ts_f32_u") (param f32) (result i32)
        (local.get 0)
        (i32.trunc_sat_f32_u)
    )
    (func (export "i32_ts_f64_s") (param f64) (result i32)
        (local.get 0)
        (i32.trunc_sat_f64_s)
    )
    (func (export "i32_ts_f64_u") (param f64) (result i32)
        (local.get 0)
        (i32.trunc_sat_f64_u)
    )
    (func (export "i64_ts_f32_s") (param f32) (result i64)
        (local.get 0)
        (i64.trunc_sat_f32_s)
    )
    (func (export "i64_ts_f32_u") (param f32) (result i64)
        (local.get 0)
        (i64.trunc_sat_f32_u)
    )
    (func (export "i64_ts_f64_s") (param f64) (result i64)
        (local.get 0)
        (i64.trunc_sat_f64_s)
    )
    (func (export "i64_ts_f64_u") (param f64) (result i64)
        (local.get 0)
        (i64.trunc_sat_f64_u)
    )
)
`

async function test() {
    const instance = await instantiate(wat, {});
    const {
        i32_ts_f32_s, i32_ts_f32_u, i32_ts_f64_s, i32_ts_f64_u,
        i64_ts_f32_s, i64_ts_f32_u, i64_ts_f64_s, i64_ts_f64_u,
    } = instance.exports

    assert.eq(i32_ts_f32_s(1.5), 1);
    assert.eq(i32_ts_f32_s(-1.5), -1);
    assert.eq(i32_ts_f32_s(-2147483649), -2147483648);
    assert.eq(i32_ts_f32_s(2147483648), 2147483647);

    assert.eq(i32_ts_f32_u(1.5), 1);
    assert.eq(i32_ts_f32_u(-2147483649), 0);
    assert.eq(i32_ts_f32_u(4294967296), -1);

    assert.eq(i32_ts_f64_s(1.5), 1);
    assert.eq(i32_ts_f64_s(-1.5), -1);
    assert.eq(i32_ts_f64_s(-2147483649), -2147483648);
    assert.eq(i32_ts_f64_s(2147483648), 2147483647);

    assert.eq(i32_ts_f64_u(1.5), 1);
    assert.eq(i32_ts_f64_u(-2147483649), 0);
    assert.eq(i32_ts_f64_u(4294967296), -1);

    assert.eq(i64_ts_f32_s(1.5), 1n);
    assert.eq(i64_ts_f32_s(-1.5), -1n);
    assert.eq(i64_ts_f32_s(-9223372036854775809), -9223372036854775808n);
    assert.eq(i64_ts_f32_s(9223372036854775808), 9223372036854775807n);

    assert.eq(i64_ts_f32_u(1.5), 1n);
    assert.eq(i64_ts_f32_u(-2147483649), 0n);
    assert.eq(i64_ts_f32_u(18446744073709551616), -1n);

    assert.eq(i64_ts_f64_s(1.5), 1n);
    assert.eq(i64_ts_f64_s(-1.5), -1n);
    assert.eq(i64_ts_f64_s(-9223372036854775809), -9223372036854775808n);
    assert.eq(i64_ts_f64_s(9223372036854775808), 9223372036854775807n);

    assert.eq(i64_ts_f64_u(1.5), 1n);
    assert.eq(i64_ts_f64_u(-2147483649), 0n);
    assert.eq(i64_ts_f64_u(18446744073709551616), -1n);
}

assert.asyncTest(test())
