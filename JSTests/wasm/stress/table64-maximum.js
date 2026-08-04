//@ skip if $addressBits <= 32
import { instantiate } from "../wabt-wrapper.js";
import * as assert from "../assert.js";

// A table64's maximum is a u64. Narrowing it to 32 bits leaves the table unable
// to grow, and when the narrowed value lands below the initial size it also
// breaks Wasm::Table's maximum >= length invariant.
for (const maximum of ["4294967296", "4294967301", "18446744073709551615"]) {
    const instance = await instantiate(`
    (module
        (table (export "table") i64 1 ${maximum} funcref)
        (func (export "size") (result i64)
            (table.size 0)
        )
        (func (export "grow") (param $delta i64) (result i64)
            (ref.null func)
            (local.get $delta)
            (table.grow 0)
        )
    )`, {}, { memory64: true });

    assert.eq(instance.exports.size(), 1n);
    assert.eq(instance.exports.grow(9n), 1n);
    assert.eq(instance.exports.size(), 10n);
    assert.eq(instance.exports.grow(0n), 10n);
}

// However large the declared maximum is, growth is still bounded by the number of
// entries a table may hold (Wasm::Table::isValidLength, maxTableEntries). Only
// rejected grows are exercised here; reaching that bound would allocate 10M entries.
{
    const instance = await instantiate(`
    (module
        (table (export "table") i64 1 18446744073709551615 funcref)
        (func (export "size") (result i64)
            (table.size 0)
        )
        (func (export "grow") (param $delta i64) (result i64)
            (ref.null func)
            (local.get $delta)
            (table.grow 0)
        )
    )`, {}, { memory64: true });

    // 9999999 lands exactly on the bound, 10000000 just past it, and the last delta
    // overflows the u64 length computation.
    for (const delta of [9999999n, 10000000n, 18446744073709551615n]) {
        assert.eq(instance.exports.grow(delta), -1n);
        assert.eq(instance.exports.size(), 1n);
    }

    assert.throws(() => instance.exports.table.grow(9999999n), RangeError, "WebAssembly.Table.prototype.grow could not grow the table");
    assert.eq(instance.exports.table.length, 1n);
}
