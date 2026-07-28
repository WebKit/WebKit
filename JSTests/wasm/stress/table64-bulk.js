//@ skip if $addressBits <= 32
//@ runDefaultWasm("-m", "--useWasmMemory64=1")
import { instantiate } from "../wabt-wrapper.js";
import * as assert from "../assert.js";

let wat = (addressType) => `
(module
    (memory ${addressType} 1)
    (table $tbl (export "table1") ${addressType} 10 funcref)
    (table $tbl2 (export "table2") ${addressType} 10 funcref)
    (func $f1 (result i32) i32.const 42)
    (func $f2 (result i32) i32.const 43)
    (elem $element funcref (ref.func $f1) (ref.func $f2))
    (func $tableInit (export "tableInit")
        (${addressType}.const 0) ;; destination
        (i32.const 0) ;; source
        (i32.const 2) ;; count 
        (table.init $tbl $element)
    )
    (func $tableFill (export "tableFill")
        (${addressType}.const 0) ;; destination
        (ref.func $f1)
        (${addressType}.const 2) ;; length
        (table.fill $tbl)
    )
    (func $tableCopy (export "tableCopy")
        (${addressType}.const 0) ;; destination
        (${addressType}.const 0) ;; source 
        (${addressType}.const 2) ;; length
        (table.copy $tbl2 $tbl)
    )
    (func $resetTables (export "resetTables")
        (${addressType}.const 0)
        (ref.null func)
        (${addressType}.const 10)
        (table.fill $tbl)
        (${addressType}.const 0)
        (ref.null func)
        (${addressType}.const 10)
        (table.fill $tbl2)
    )
)`;

function test(initFunc, fillFunc, copyFunc, resetFunc, table1, table2, numOrBig) {
    resetFunc();
    assert.eq(table1.get(numOrBig(0)), null);
    assert.eq(table1.get(numOrBig(1)), null);
    assert.eq(table2.get(numOrBig(0)), null);
    assert.eq(table2.get(numOrBig(1)), null);
    initFunc();
    assert.eq(table1.get(numOrBig(0))(), 42);
    assert.eq(table1.get(numOrBig(1))(), 43);
    fillFunc();
    assert.eq(table1.get(numOrBig(0))(), 42);
    assert.eq(table1.get(numOrBig(1))(), 42);
    copyFunc();
    assert.eq(table2.get(numOrBig(0))(), 42);
    assert.eq(table2.get(numOrBig(1))(), 42);
}

for (const addressType of ["i32", "i64"]) {
    const instance = await instantiate(wat(addressType), {}, {memory64: true});
    const { tableInit, tableFill, tableCopy, resetTables, table1, table2 } = instance.exports;
    const numOrBig = (val) => addressType == "i32" ? Number(val) : BigInt(val);
    for (let i = 0; i < wasmTestLoopCount; i++)
        test(tableInit, tableFill, tableCopy, resetTables, table1, table2, numOrBig);
}
