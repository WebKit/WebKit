//@ skip if $addressBits <= 32
//@ runDefaultWasm("-m", "--useWasmMemory64=1", "--useBBQJIT=0", "--useOMGJIT=0")
import { instantiate } from "../wabt-wrapper.js";
import * as assert from "../assert.js";

let wat = (addressType) => `
(module
    (memory ${addressType} 1)
    (table $tbl ${addressType} 1 funcref)
    (func $tableGrow (export "tableGrow") (param $delta ${addressType}) (result ${addressType})
        (ref.null func)
        (local.get $delta)
        (table.grow $tbl)
    )
    (func $tableSize (export "tableSize") (result ${addressType})
        (table.size $tbl)
    )
)`;

function test(growFunc, sizeFunc, numOrBig) {
    let size = numOrBig(0);
    for (let i = 0; i < wasmTestLoopCount; i++)
        size = sizeFunc();
    assert.eq(size, numOrBig(1));

    let growSize = growFunc(numOrBig(10));
    assert.eq(growSize, size);
    assert.eq(sizeFunc(), numOrBig(11));
    size = sizeFunc();

    growSize = growFunc(numOrBig(10));
    assert.eq(growSize, size);
    assert.eq(sizeFunc(), numOrBig(21));
    size = sizeFunc();

    growSize = growFunc(numOrBig(10));
    assert.eq(growSize, size);
    assert.eq(sizeFunc(), numOrBig(31));
}

for (const addressType of ["i32", "i64"]) {
    const instance = await instantiate(wat(addressType), {}, {memory64: true});
    const { tableGrow, tableSize } = instance.exports;
    const numOrBig = (val) => addressType == "i32" ? Number(val) : BigInt(val);
    test(tableGrow, tableSize, numOrBig);
}

