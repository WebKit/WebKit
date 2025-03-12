import { instantiate } from "../wabt-wrapper.js";
import * as assert from "../assert.js";

let wat = `
(module
    (func (export "rotl32_const") (param i32) (result i32)
        local.get 0
        i32.const 1
        i32.shl
        local.get 0
        i32.const 31
        i32.shr_u
        i32.or
    )
    (func (export "rotl32") (param i32 i32) (result i32)
        local.get 0
        local.get 1
        i32.shl
        local.get 0
        i32.const 32
        local.get 1
        i32.sub
        i32.shr_u
        i32.or
    )
    (func (export "rotr32_const") (param i32) (result i32)
        local.get 0
        i32.const 1
        i32.shr_u
        local.get 0
        i32.const 31
        i32.shl
        i32.or
    )
    (func (export "rotr32") (param i32 i32) (result i32)
        local.get 0
        local.get 1
        i32.shr_u
        local.get 0
        i32.const 32
        local.get 1
        i32.sub
        i32.shl
        i32.or
    )
    (func (export "rotl64_const") (param i64) (result i64)
        local.get 0
        i64.const 1
        i64.shl
        local.get 0
        i64.const 63
        i64.shr_u
        i64.or
    )
    (func (export "rotl64") (param i64 i64) (result i64)
        local.get 0
        local.get 1
        i64.shl
        local.get 0
        i64.const 64
        local.get 1
        i64.sub
        i64.shr_u
        i64.or
    )
    (func (export "rotr64_const") (param i64) (result i64)
        local.get 0
        i64.const 1
        i64.shr_u
        local.get 0
        i64.const 63
        i64.shl
        i64.or
    )
    (func (export "rotr64") (param i64 i64) (result i64)
        local.get 0
        local.get 1
        i64.shr_u
        local.get 0
        i64.const 64
        local.get 1
        i64.sub
        i64.shl
        i64.or
    )
    (func (export "not_rotl32_const") (param i32) (result i32)
        local.get 0
        i32.const 1
        i32.shl
        local.get 0
        i32.const 30
        i32.shr_u
        i32.or
    )
    (func (export "not_rotl32") (param i32 i32) (result i32)
        local.get 0
        local.get 1
        i32.shl
        local.get 0
        i32.const 31
        local.get 1
        i32.sub
        i32.shr_u
        i32.or
    )
    (func (export "not_rotl64_const") (param i64) (result i64)
        local.get 0
        i64.const 1
        i64.shl
        local.get 0
        i64.const 62
        i64.shr_u
        i64.or
    )
    (func (export "not_rotl64") (param i64 i64) (result i64)
        local.get 0
        local.get 1
        i64.shl
        local.get 0
        i64.const 63
        local.get 1
        i64.sub
        i64.shr_u
        i64.or
    )
)
`;

async function test() {
    const instance = await instantiate(wat, {}, {});
    const {
        rotl32_const, rotl32,
        rotr32_const, rotr32,
        rotl64_const, rotl64,
        rotr64_const, rotr64,
        not_rotl32_const, not_rotl32,
        not_rotl64_const, not_rotl64
    } = instance.exports;

    for (let i = 0; i < 50000; i ++) {
        assert.eq(rotl32_const(1), 2);
        assert.eq(rotl32_const(-0x80000000), 1);
        assert.eq(rotl32(1, 1), 2);
        assert.eq(rotl32(-0x80000000, 1), 1);
        assert.eq(rotr32_const(2), 1);
        assert.eq(rotr32_const(1), -0x80000000);
        assert.eq(rotr32(2, 1), 1);
        assert.eq(rotr32(1, 1), -0x80000000);

        assert.eq(rotl64_const(1n), 2n);
        assert.eq(rotl64_const(-0x8000000000000000n), 1n);
        assert.eq(rotl64(1n, 1n), 2n);
        assert.eq(rotl64(-0x8000000000000000n, 1n), 1n);
        assert.eq(rotr64_const(2n), 1n);
        assert.eq(rotr64_const(1n), -0x8000000000000000n);
        assert.eq(rotr64(2n, 1n), 1n);
        assert.eq(rotr64(1n, 1n), -0x8000000000000000n);

        assert.eq(not_rotl32_const(-0x80000000), 2);
        assert.eq(not_rotl32(-0x80000000, 1), 2);
        assert.eq(not_rotl64_const(-0x8000000000000000n), 2n);
        assert.eq(not_rotl64(-0x8000000000000000n, 1n), 2n);
    }
}

await assert.asyncTest(test());
