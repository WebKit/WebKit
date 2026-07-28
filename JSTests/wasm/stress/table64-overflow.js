//@ skip if $addressBits <= 32
//@ runDefaultWasm("-m", "--useWasmMemory64=1")
import { instantiate } from "../wabt-wrapper.js";
import * as assert from "../assert.js";

let wat = `
(module
    (memory i64 1)
    (table $tbl i64 100 externref)
    (table $tbl2 i64 100 externref)
    (table $tblFunc i64 100 funcref)
    (table $tblGrow i64 1 funcref)
    (func $f (result i32) i32.const 42)
    (elem $element funcref (ref.func $f))
    (func $tableGet (export "tableGet") (param $index i64) (result externref)
        (local.get $index)
        (table.get $tbl)
    )
    (func $tableSet (export "tableSet") (param $index i64) (param $value externref)
        (local.get $index)
        (local.get $value)
        (table.set $tbl)
    )
    (func $tableFill (export "tableFill") (param $offset i64) (param $count i64)
        (local.get $offset)
        (ref.null extern)
        (local.get $count)
        (table.fill $tbl)
    )
    (func $tableCopy (export "tableCopy") (param $dst i64) (param $src i64) (param $len i64)
        (local.get $dst)
        (local.get $src)
        (local.get $len)
        (table.copy $tbl2 $tbl)
    )
    (func $tableInit (export "tableInit") (param $dst i64) (param $src i32) (param $len i32)
        (local.get $dst)
        (local.get $src)
        (local.get $len)
        (table.init $tblFunc $element)
    )
    (func $tableGrow (export "tableGrow") (param $delta i64) (result i64)
        (ref.null func)
        (local.get $delta)
        (table.grow $tblGrow)
    )
    (func $tableGrowSize (export "tableGrowSize") (result i64)
        (table.size $tblGrow)
    )
)`;

const oob = "Out of bounds table access";
const twoPow32 = 0x1_0000_0000n;

function test(exports) {
    const { tableGet, tableSet, tableFill, tableCopy, tableInit, tableGrow, tableGrowSize } = exports;

    assert.throws(() => tableGet(twoPow32 + 5n), WebAssembly.RuntimeError, oob);
    assert.throws(() => tableGet(twoPow32 + 100n), WebAssembly.RuntimeError, oob);

    assert.throws(() => tableSet(twoPow32 + 5n, "x"), WebAssembly.RuntimeError, oob);
    assert.eq(tableGet(5n), null);

    assert.throws(() => tableFill(twoPow32, 1n), WebAssembly.RuntimeError, oob);
    assert.throws(() => tableFill(0n, twoPow32 + 1n), WebAssembly.RuntimeError, oob);
    assert.eq(tableGet(0n), null);

    assert.throws(() => tableCopy(twoPow32, 0n, 1n), WebAssembly.RuntimeError, oob);
    assert.throws(() => tableCopy(0n, twoPow32, 1n), WebAssembly.RuntimeError, oob);
    assert.throws(() => tableCopy(0n, 0n, twoPow32 + 1n), WebAssembly.RuntimeError, oob);

    assert.throws(() => tableInit(twoPow32, 0, 1), WebAssembly.RuntimeError, oob);

    const badGrow = tableGrow(twoPow32 + 5n);
    assert.eq(badGrow, -1n);
    assert.eq(tableGrowSize(), 1n);
}

const instance = await instantiate(wat, {}, {memory64: true});
for (let i = 0; i < wasmTestLoopCount; i++)
    test(instance.exports);
