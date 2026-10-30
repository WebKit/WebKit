import * as assert from '../assert.js';
import { instantiate } from "../wabt-wrapper.js";

function module_wat() {
    let acc = '(module ';
    for (const op of ["rotl", "rotr"]) {
        acc += `(func (export "${op}") (param i64) (result i64)
                  i64.const 0x1122334455667788
                  local.get 0
                  i64.const 0
                  i64.xor
                  i64.${op})`
    }
    acc += ")"
    return acc;
}

const mask64 = (1n << 64n) - 1n;
function rotl(num, amount) {
    num = BigInt.asUintN(64, num);
    amount = BigInt.asUintN(64, amount);

    amount &= 63n;
    if (amount == 0)
        return num;
    const amount_complement = 64n - amount;
    return ((num << amount) | (num >> amount_complement)) & mask64;
}

function rotr(num, amount) {
    return rotl(num, 64n - (amount & 63n));
}

const reference = {rotl, rotr};

const instance = await instantiate(module_wat(), {});
for (let i = 0; i < wasmTestLoopCount; ++i) {
    for (const op of ["rotl", "rotr"]) {
        for (const amount of [0, 1, 4, 8, 15, 16, 31, 32, 33, 63, 64, 65]) {
            const ref = reference[op](0x1122334455667788n, BigInt(amount));
            const ret = BigInt.asUintN(64, instance.exports[op](BigInt(amount)));
            assert.eq(ret, ref);
        }
    }
}
