import * as assert from '../assert.js';
import { instantiate } from "../wabt-wrapper.js";

let wat = `
(module
  (func (export "test") (param i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64) (result i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64)
    local.get 0
    local.get 1
    local.get 2
    local.get 3
    local.get 4
    local.get 5
    local.get 6
    local.get 7
    local.get 8
    local.get 9
    local.get 10
    local.get 11
    local.get 12
    local.get 13
    local.get 14
    local.get 15
    local.get 16
    local.get 17
    local.get 18
    local.get 19
    local.get 20
    local.get 21
    local.get 22
    local.get 23
    local.get 24
    local.get 25
    local.get 26
    local.get 27
    local.get 28
    local.get 29
    local.get 30
    local.get 31
    local.get 32
    local.get 33
    local.get 34
    local.get 35
    local.get 36
    local.get 37
    local.get 38
    local.get 39
    local.get 40
    local.get 41
    local.get 42
    local.get 43
    local.get 44
    local.get 45
    local.get 46
    local.get 47
    local.get 48
    local.get 49
    local.get 50
    local.get 51
    local.get 52
    local.get 53
    local.get 54
    local.get 55
    local.get 56
    local.get 57
    local.get 58
    local.get 59
  )
)
`;

async function test() {
    let instance = await instantiate(wat);
    let result = instance.exports.test(
        0n, 1n, 2n, 3n, 4n, 5n, 6n, 7n, 8n, 9n,
        10n, 11n, 12n, 13n, 14n, 15n, 16n, 17n, 18n, 19n,
        20n, 21n, 22n, 23n, 24n, 25n, 26n, 27n, 28n, 29n,
        30n, 31n, 32n, 33n, 34n, 35n, 36n, 37n, 38n, 39n,
        40n, 41n, 42n, 43n, 44n, 45n, 46n, 47n, 48n, 49n,
        50n, 51n, 52n, 53n, 54n, 55n, 56n, 57n, 58n, 59n);
    assert.eq(result.length, 60);
    for (let i = 0; i < 60; ++i) {
        assert.eq(result[i], BigInt(i));
    }
}

assert.asyncTest(test());
