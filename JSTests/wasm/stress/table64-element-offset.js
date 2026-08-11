//@ skip if $addressBits <= 32
import { instantiate } from "../wabt-wrapper.js";
import * as assert from "../assert.js";

const options = { memory64: true };

// An active element segment on a table64 has a u64 offset. Truncating it to 32
// bits turns an out-of-bounds segment into one that silently initializes the
// wrong slots instead of failing instantiation.
for (const offset of ["4294967296", "4294967301", "18446744073709551615"]) {
    await assert.throwsAsync(
        instantiate(`
        (module
            (table i64 10 funcref)
            (elem (i64.const ${offset}) $f)
            (func $f)
        )`, {}, options),
        WebAssembly.RuntimeError,
        "Element is trying to set an out of bounds table index");
}

// Same, with the offset coming from an imported i64 global.
await assert.throwsAsync(
    instantiate(`
    (module
        (import "m" "g" (global $g i64))
        (table i64 10 funcref)
        (elem (global.get $g) $f)
        (func $f)
    )`, { m: { g: new WebAssembly.Global({ value: "i64" }, 4294967296n) } }, options),
    WebAssembly.RuntimeError,
    "Element is trying to set an out of bounds table index");

// An in-bounds offset above 2^32 is impossible, but offsets that fit must still
// work, and an empty segment is allowed to start exactly at the table's end.
{
    const instance = await instantiate(`
    (module
        (table (export "table") i64 10 funcref)
        (elem (i64.const 7) $f)
        (elem (i64.const 10))
        (type $ft (func (result i32)))
        (func $f (result i32)
            (i32.const 42)
        )
        (func (export "callAt") (param $i i64) (result i32)
            (local.get $i)
            (call_indirect (type $ft))
        )
    )`, {}, options);

    assert.eq(instance.exports.callAt(7n), 42);
    assert.eq(instance.exports.table.get(0n), null);
}
