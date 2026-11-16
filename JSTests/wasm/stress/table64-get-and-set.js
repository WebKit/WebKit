//@ skip if $addressBits <= 32
import { instantiate } from "../wabt-wrapper.js";
import * as assert from "../assert.js";

let wat = (addressType) => `
(module
    (memory ${addressType} 1)
    (table $tbl ${addressType} 100 externref)
    (func $tableSet (export "tableSet") (param $index ${addressType}) (param $setValue externref)
        (local.get $index)
        (local.get $setValue)
        (table.set $tbl)
    )
    (func $tableGet (export "tableGet") (param $index ${addressType}) (result externref)
        (local.get $index)
        (table.get $tbl)
    )
)`;

function test(getFunc, setFunc, index) {
    const valueToSet = "hello";
    setFunc(index, valueToSet);
    assert.eq(getFunc(index), valueToSet);
}

for (const addressType of ["i32", "i64"]) {
    const instance = await instantiate(wat(addressType), {}, {memory64: true});
    const { tableGet, tableSet } = instance.exports;
    const index = addressType == "i32" ? 42 : 42n;
    for (let i = 0; i < wasmTestLoopCount; i++)
        test(tableGet, tableSet, index);
}

