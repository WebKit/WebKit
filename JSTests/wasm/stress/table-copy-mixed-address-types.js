//@ skip if $addressBits <= 32
import { instantiate } from "../wabt-wrapper.js";
import * as assert from "../assert.js";

// table.copy between tables with different address types: each offset is typed
// by its own table, and the length by the narrower of the two.
let wat = (dstAddressType, srcAddressType) => `
(module
    (table $dst (export "dst") ${dstAddressType} 8 funcref)
    (table $src ${srcAddressType} 8 funcref)
    (elem (table $src) (${srcAddressType}.const 1) func $f)
    (func $f (result i32)
        (i32.const 42)
    )
    (type $ft (func (result i32)))
    (func (export "copy") (param $dstOffset ${dstAddressType}) (param $srcOffset ${srcAddressType}) (param $length ${dstAddressType === "i64" && srcAddressType === "i64" ? "i64" : "i32"})
        (local.get $dstOffset)
        (local.get $srcOffset)
        (local.get $length)
        (table.copy $dst $src)
    )
    (func (export "callDst") (param $i ${dstAddressType}) (result i32)
        (local.get $i)
        (call_indirect $dst (type $ft))
    )
)`;

for (const dstAddressType of ["i32", "i64"]) {
    for (const srcAddressType of ["i32", "i64"]) {
        const instance = await instantiate(wat(dstAddressType, srcAddressType), {}, { memory64: true });
        const dstNum = dstAddressType === "i32" ? Number : BigInt;
        const srcNum = srcAddressType === "i32" ? Number : BigInt;
        const lengthNum = dstAddressType === "i64" && srcAddressType === "i64" ? BigInt : Number;

        for (let i = 0; i < wasmTestLoopCount; ++i) {
            instance.exports.copy(dstNum(3), srcNum(1), lengthNum(1));
            assert.eq(instance.exports.callDst(dstNum(3)), 42);
        }
    }
}
