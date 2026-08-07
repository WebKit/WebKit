//@ skip if $addressBits <= 32
import { instantiate } from "../wabt-wrapper.js";
import * as assert from "../assert.js";

// Supplying one JS table for two table imports makes two table indices name the same table, so
// table.copy between those indices has to behave like memmove.

let wat = (addressType) => `
(module
    (import "m" "a" (table $a ${addressType} 10 externref))
    (import "m" "b" (table $b ${addressType} 10 externref))
    (func (export "copyAB") (param $d ${addressType}) (param $s ${addressType}) (param $n ${addressType})
        (local.get $d)
        (local.get $s)
        (local.get $n)
        (table.copy $a $b)
    )
    (func (export "copyAA") (param $d ${addressType}) (param $s ${addressType}) (param $n ${addressType})
        (local.get $d)
        (local.get $s)
        (local.get $n)
        (table.copy $a $a)
    )
)`;

function reset(table, numOrBig) {
    for (let i = 0; i < 10; ++i)
        table.set(numOrBig(i), "v" + i);
}

function contents(table, numOrBig) {
    let result = [];
    for (let i = 0; i < 10; ++i)
        result.push(table.get(numOrBig(i)));
    return result.join(",");
}

function test(copy, table, numOrBig, d, s, n, expected) {
    reset(table, numOrBig);
    copy(numOrBig(d), numOrBig(s), numOrBig(n));
    assert.eq(contents(table, numOrBig), expected);
}

for (const addressType of ["i32", "i64"]) {
    const numOrBig = (val) => addressType == "i32" ? Number(val) : BigInt(val);
    const table = new WebAssembly.Table({ element: "externref", initial: numOrBig(10), address: addressType });
    const instance = await instantiate(wat(addressType), { m: { a: table, b: table } }, { memory64: true });
    const { copyAB, copyAA } = instance.exports;

    for (let i = 0; i < wasmTestLoopCount; i++) {
        for (const copy of [copyAB, copyAA]) {
            test(copy, table, numOrBig, 2, 0, 5, "v0,v1,v0,v1,v2,v3,v4,v7,v8,v9");
            test(copy, table, numOrBig, 0, 2, 5, "v2,v3,v4,v5,v6,v5,v6,v7,v8,v9");
            test(copy, table, numOrBig, 2, 2, 5, "v0,v1,v2,v3,v4,v5,v6,v7,v8,v9");
            test(copy, table, numOrBig, 0, 0, 0, "v0,v1,v2,v3,v4,v5,v6,v7,v8,v9");
        }
    }
}
