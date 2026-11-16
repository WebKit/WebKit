//@ skip if $addressBits <= 32
import { instantiate } from "../wabt-wrapper.js";
import * as assert from "../assert.js";

let wat = (addressType) => `
(module
    (memory ${addressType} 1)
    (table $tbl ${addressType} 10 funcref)
    (type $func_type (func (param i32 i32) (result i32)))
    (elem (${addressType}.const 0) $addIndirect $subIndirect)
    (func $add (export "add") (param $lhs i32) (param $rhs i32) (result i32)
        (local.get $lhs)
        (local.get $rhs)
        (${addressType}.const 0)
        (call_indirect $tbl (type $func_type))
    )
    (func $sub (export "sub") (param $lhs i32) (param $rhs i32) (result i32)
        (local.get $lhs)
        (local.get $rhs)
        (${addressType}.const 1)
        (call_indirect $tbl (type $func_type))
    )
    (func $callAt (export "callAt") (param $lhs i32) (param $rhs i32) (param $index ${addressType}) (result i32)
        (local.get $lhs)
        (local.get $rhs)
        (local.get $index)
        (call_indirect $tbl (type $func_type))
    )
    (func $addIndirect (param $lhs i32) (param $rhs i32) (result i32)
        (local.get $lhs)
        (local.get $rhs)
        (i32.add)
    )
    (func $subIndirect (param $lhs i32) (param $rhs i32) (result i32)
        (local.get $lhs)
        (local.get $rhs)
        (i32.sub)
    )
)`;

function test(addFunc, subFunc, callAt) {
    assert.eq(addFunc(12, 30), 42);
    assert.eq(subFunc(53, 11), 42);
    assert.eq(callAt(12, 30, addressIndex(0)), 42);
    assert.eq(callAt(53, 11, addressIndex(1)), 42);
}

const oob = "Out of bounds call_indirect";
const twoPow32 = 0x1_0000_0000n;
let addressIndex;

for (const addressType of ["i32", "i64"]) {
    const instance = await instantiate(wat(addressType), {}, {memory64: true});
    const { add, sub, callAt } = instance.exports;
    addressIndex = addressType === "i64" ? (i) => BigInt(i) : (i) => i;
    for (let i = 0; i < wasmTestLoopCount; i++)
        test(add, sub, callAt);

    // A table64 index above int32_t::max must trap rather than truncate to a valid slot.
    if (addressType === "i64") {
        assert.throws(() => callAt(12, 30, twoPow32), WebAssembly.RuntimeError, oob);
        assert.throws(() => callAt(12, 30, twoPow32 + 5n), WebAssembly.RuntimeError, oob);
    }
}

