import { instantiate } from "../wabt-wrapper.js";
import * as assert from "../assert.js";

// BBQ strength-reduces constant div/rem when |divisor| is a power of two.
// Cover negative signed divisors (-2, -4, -32, …) as well as rem_s sign rules.
let wat = `
(module
    (func (export "divS_i32_neg2") (param i32) (result i32)
        local.get 0
        i32.const -2
        i32.div_s)
    (func (export "remS_i32_neg2") (param i32) (result i32)
        local.get 0
        i32.const -2
        i32.rem_s)
    (func (export "divS_i32_neg4") (param i32) (result i32)
        local.get 0
        i32.const -4
        i32.div_s)
    (func (export "remS_i32_neg4") (param i32) (result i32)
        local.get 0
        i32.const -4
        i32.rem_s)
    (func (export "divS_i32_neg32") (param i32) (result i32)
        local.get 0
        i32.const -32
        i32.div_s)
    (func (export "remS_i32_neg32") (param i32) (result i32)
        local.get 0
        i32.const -32
        i32.rem_s)
    (func (export "divS_i64_neg2") (param i64) (result i64)
        local.get 0
        i64.const -2
        i64.div_s)
    (func (export "remS_i64_neg2") (param i64) (result i64)
        local.get 0
        i64.const -2
        i64.rem_s)
    (func (export "divS_i64_neg8") (param i64) (result i64)
        local.get 0
        i64.const -8
        i64.div_s)
    (func (export "remS_i64_neg8") (param i64) (result i64)
        local.get 0
        i64.const -8
        i64.rem_s)
    (func (export "divS_i32_min") (param i32) (result i32)
        local.get 0
        i32.const -2147483648
        i32.div_s)
    (func (export "remS_i32_min") (param i32) (result i32)
        local.get 0
        i32.const -2147483648
        i32.rem_s)
    (func (export "divS_i64_min") (param i64) (result i64)
        local.get 0
        i64.const -9223372036854775808
        i64.div_s)
    (func (export "remS_i64_min") (param i64) (result i64)
        local.get 0
        i64.const -9223372036854775808
        i64.rem_s)
)
`;

async function test() {
    const instance = await instantiate(wat, {}, {});
    const {
        divS_i32_neg2, remS_i32_neg2,
        divS_i32_neg4, remS_i32_neg4,
        divS_i32_neg32, remS_i32_neg32,
        divS_i64_neg2, remS_i64_neg2,
        divS_i64_neg8, remS_i64_neg8,
        divS_i32_min, remS_i32_min,
        divS_i64_min, remS_i64_min,
    } = instance.exports;

    const i32Cases = [0, 1, 2, 3, 7, 8, 9, -1, -2, -3, -7, -8, -9, 100, -100, 0x7fffffff, -0x80000000];
    const i64Cases = [0n, 1n, 2n, 7n, 8n, 9n, -1n, -2n, -7n, -8n, -9n, 100n, -100n, (1n << 62n), -(1n << 62n), -(1n << 63n)];
    const i32Min = -2147483648;
    const i64Min = -(1n << 63n);

    for (let i = 0; i < wasmTestLoopCount; ++i) {
        for (const n of i32Cases) {
            assert.eq(divS_i32_neg2(n), (n / -2) | 0);
            assert.eq(remS_i32_neg2(n), (n % -2) | 0);
            assert.eq(divS_i32_neg4(n), (n / -4) | 0);
            assert.eq(remS_i32_neg4(n), (n % -4) | 0);
            assert.eq(divS_i32_neg32(n), (n / -32) | 0);
            assert.eq(remS_i32_neg32(n), (n % -32) | 0);
            assert.eq(divS_i32_min(n), (n / i32Min) | 0);
            assert.eq(remS_i32_min(n), (n % i32Min) | 0);
        }

        for (const n of i64Cases) {
            assert.eq(divS_i64_neg2(n), n / -2n);
            assert.eq(remS_i64_neg2(n), n % -2n);
            assert.eq(divS_i64_neg8(n), n / -8n);
            assert.eq(remS_i64_neg8(n), n % -8n);
            assert.eq(divS_i64_min(n), n / i64Min);
            assert.eq(remS_i64_min(n), n % i64Min);
        }
    }
}

await assert.asyncTest(test());
