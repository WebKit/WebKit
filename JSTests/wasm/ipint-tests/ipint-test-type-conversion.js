import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (func (export "i32_wrap_i64") (param i64) (result i32)
        (local.get 0)
        (i32.wrap_i64)
    )

    (func (export "i32_trunc_f32_s") (param f32) (result i32)
        (local.get 0)
        (i32.trunc_f32_s)
    )
    (func (export "i32_trunc_f32_u") (param f32) (result i32)
        (local.get 0)
        (i32.trunc_f32_u)
    )
    (func (export "i32_trunc_f64_s") (param f64) (result i32)
        (local.get 0)
        (i32.trunc_f64_s)
    )
    (func (export "i32_trunc_f64_u") (param f64) (result i32)
        (local.get 0)
        (i32.trunc_f64_u)
    )

    (func (export "i64_extend_i32_s") (param i32) (result i64)
        (local.get 0)
        (i64.extend_i32_s)
    )
    (func (export "i64_extend_i32_u") (param i32) (result i64)
        (local.get 0)
        (i64.extend_i32_u)
    )

    (func (export "i64_trunc_f32_s") (param f32) (result i64)
        (local.get 0)
        (i64.trunc_f32_s)
    )
    (func (export "i64_trunc_f32_u") (param f32) (result i64)
        (local.get 0)
        (i64.trunc_f32_u)
    )
    (func (export "i64_trunc_f64_s") (param f64) (result i64)
        (local.get 0)
        (i64.trunc_f64_s)
    )
    (func (export "i64_trunc_f64_u") (param f64) (result i64)
        (local.get 0)
        (i64.trunc_f64_u)
    )

    (func (export "f32_convert_i32_s") (param i32) (result f32)
        (local.get 0)
        (f32.convert_i32_s)
    )
    (func (export "f32_convert_i32_u") (param i32) (result f32)
        (local.get 0)
        (f32.convert_i32_u)
    )
    (func (export "f32_convert_i64_s") (param i64) (result f32)
        (local.get 0)
        (f32.convert_i64_s)
    )
    (func (export "f32_convert_i64_u") (param i64) (result f32)
        (local.get 0)
        (f32.convert_i64_u)
    )

    (func (export "f32_demote_f64") (param f64) (result f32)
        (local.get 0)
        (f32.demote_f64)
    )

    (func (export "f64_convert_i32_s") (param i32) (result f64)
        (local.get 0)
        (f64.convert_i32_s)
    )
    (func (export "f64_convert_i32_u") (param i32) (result f64)
        (local.get 0)
        (f64.convert_i32_u)
    )
    (func (export "f64_convert_i64_s") (param i64) (result f64)
        (local.get 0)
        (f64.convert_i64_s)
    )
    (func (export "f64_convert_i64_u") (param i64) (result f64)
        (local.get 0)
        (f64.convert_i64_u)
    )

    (func (export "f64_promote_f32") (param f32) (result f64)
        (local.get 0)
        (f64.promote_f32)
    )

    (func (export "i32_reinterpret_f32") (param f32) (result i32)
        (local.get 0)
        (i32.reinterpret_f32)
    )
    (func (export "i64_reinterpret_f64") (param f64) (result i64)
        (local.get 0)
        (i64.reinterpret_f64)
    )
    (func (export "f32_reinterpret_i32") (param i32) (result f32)
        (local.get 0)
        (f32.reinterpret_i32)
    )
    (func (export "f64_reinterpret_i64") (param i64) (result f64)
        (local.get 0)
        (f64.reinterpret_i64)
    )

    (func (export "i32_extend8_s") (param i32) (result i32)
        (local.get 0)
        (i32.extend8_s)
    )
    (func (export "i32_extend16_s") (param i32) (result i32)
        (local.get 0)
        (i32.extend16_s)
    )

    (func (export "i64_extend8_s") (param i64) (result i64)
        (local.get 0)
        (i64.extend8_s)
    )
    (func (export "i64_extend16_s") (param i64) (result i64)
        (local.get 0)
        (i64.extend16_s)
    )
    (func (export "i64_extend32_s") (param i64) (result i64)
        (local.get 0)
        (i64.extend32_s)
    )
)
`

function close(a, b) {
    return Math.abs(a - b) < 0.001;
}

let assertClose = (x, y) => assert.truthy(close(x, y))

async function test() {
    const instance = await instantiate(wat, {});
    const {
        i32_wrap_i64,
        i32_trunc_f32_s, i32_trunc_f32_u, i32_trunc_f64_s, i32_trunc_f64_u,
        i64_extend_i32_s, i64_extend_i32_u,
        i64_trunc_f32_s, i64_trunc_f32_u, i64_trunc_f64_s, i64_trunc_f64_u,
        f32_convert_i32_s, f32_convert_i32_u, f32_convert_i64_s, f32_convert_i64_u,
        f32_demote_f64,
        f64_convert_i32_s, f64_convert_i32_u, f64_convert_i64_s, f64_convert_i64_u,
        f64_promote_f32,
        i32_reinterpret_f32, i64_reinterpret_f64, f32_reinterpret_i32, f64_reinterpret_i64,
        i32_extend8_s,
        i32_extend16_s,
        i64_extend8_s,
        i64_extend16_s,
        i64_extend32_s,
    } = instance.exports;

    assert.eq(i32_wrap_i64(5n), 5);
    assert.eq(i32_wrap_i64(-16n), -16);

    assert.eq(i32_trunc_f32_s(1.65), 1);
    assert.eq(i32_trunc_f32_s(-2.5), -2);

    assert.eq(i32_trunc_f32_u(1.65), 1);
    assert.eq(i32_trunc_f32_u(2.5), 2);

    assert.eq(i32_trunc_f64_s(1.65), 1);
    assert.eq(i32_trunc_f64_s(-2.25), -2);

    assert.eq(i32_trunc_f64_u(1.65), 1);
    assert.eq(i32_trunc_f64_u(2.5), 2);

    assert.eq(i64_extend_i32_s(16), 16n);
    assert.eq(i64_extend_i32_s(-16), -16n);

    assert.eq(i64_extend_i32_u(16), 16n);
    assert.eq(i64_extend_i32_u(2147483647), 2147483647n);

    assert.eq(i64_trunc_f32_s(1.65), 1n);
    assert.eq(i64_trunc_f32_s(-2.5), -2n);

    assert.eq(i64_trunc_f32_u(1.65), 1n);
    assert.eq(i64_trunc_f32_u(2.5), 2n);

    assert.eq(i64_trunc_f64_s(1.65), 1n);
    assert.eq(i64_trunc_f64_s(-2.25), -2n);

    assert.eq(i64_trunc_f64_u(1.65), 1n);
    assert.eq(i64_trunc_f64_u(2.5), 2n);

    assertClose(f32_convert_i32_s(16), 16);
    assertClose(f32_convert_i32_s(-2), -2);

    assertClose(f32_convert_i32_u(5), 5);
    assertClose(f32_convert_i32_u(16), 16);

    assertClose(f32_convert_i64_s(16n), 16);
    assertClose(f32_convert_i64_s(-2n), -2);

    assertClose(f32_convert_i64_u(5n), 5);
    assertClose(f32_convert_i64_u(16n), 16);

    assertClose(f32_demote_f64(0.5), 0.5);
    assertClose(f32_demote_f64(-0.25), -0.25);

    assertClose(f64_convert_i32_s(16), 16);
    assertClose(f64_convert_i32_s(-2), -2);

    assertClose(f64_convert_i32_u(5), 5);
    assertClose(f64_convert_i32_u(16), 16);

    assertClose(f64_convert_i64_s(16n), 16);
    assertClose(f64_convert_i64_s(-2n), -2);

    assertClose(f64_convert_i64_u(5n), 5);
    assertClose(f64_convert_i64_u(16n), 16);

    assertClose(f64_promote_f32(0.5), 0.5);
    assertClose(f64_promote_f32(-0.25), -0.25);

    assert.eq(i32_reinterpret_f32(0.25), 0x3e800000);
    assert.eq(i32_reinterpret_f32(0.125), 0x3e000000);

    assert.eq(i64_reinterpret_f64(0.25), 0x3fd0000000000000n);
    assert.eq(i64_reinterpret_f64(0.125), 0x3fc0000000000000n);

    assertClose(f32_reinterpret_i32(0x3e800000), 0.25);
    assertClose(f32_reinterpret_i32(0x3e000000), 0.125);

    assertClose(f64_reinterpret_i64(0x3fd0000000000000n), 0.25);
    assertClose(f64_reinterpret_i64(0x3fc0000000000000n), 0.125);

    assert.eq(i32_extend8_s(16), 16);
    assert.eq(i32_extend8_s(255), -1);

    assert.eq(i32_extend16_s(16), 16);
    assert.eq(i32_extend16_s(32768), -32768);

    assert.eq(i64_extend8_s(16n), 16n);
    assert.eq(i64_extend8_s(255n), -1n);

    assert.eq(i64_extend16_s(16n), 16n);
    assert.eq(i64_extend16_s(32768n), -32768n);

    assert.eq(i64_extend32_s(16n), 16n);
    assert.eq(i64_extend32_s(4294967295n), -1n);
}

assert.asyncTest(test())
