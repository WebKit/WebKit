//@ requireOptions("--useWasmJSTypes=1")
import * as assert from "../assert.js";

// The JS API surface for i64 memories and tables: error types, and the address type of reported
// sizes. https://webassembly.github.io/memory64/js-api/

// A bad `address` is a failed WebIDL enum conversion, which is a TypeError.
for (const address of ["i65", "", "I64", "u64", null, 1, {}]) {
    assert.throws(() => new WebAssembly.Memory({ initial: 1n, address }), TypeError,
        "WebAssembly.Memory 'address' must be a string of value 'i32' or 'i64'");
    assert.throws(() => new WebAssembly.Table({ initial: 1n, element: "externref", address }), TypeError,
        "WebAssembly.Table 'address' must be a string of value 'i32' or 'i64'");
}

// An absent or undefined `address` defaults to i32, which keeps taking Numbers.
{
    const mem = new WebAssembly.Memory({ initial: 1 });
    assert.eq(mem.type().address, "i32");
    assert.eq(typeof mem.grow(1), "number");
    assert.eq(new WebAssembly.Memory({ initial: 1, address: undefined }).type().address, "i32");
}

// An i64 memory takes and returns BigInts; a Number is a TypeError.
{
    const mem = new WebAssembly.Memory({ initial: 1n, maximum: 4n, address: "i64" });
    assert.eq(mem.type().address, "i64");
    assert.eq(mem.type().minimum, 1n);
    assert.eq(mem.type().maximum, 4n);
    assert.eq(typeof mem.grow(1n), "bigint");
    assert.eq(mem.grow(0n), 2n);
    assert.throws(() => mem.grow(1), TypeError, "Invalid argument type in ToBigInt operation");
    assert.throws(() => new WebAssembly.Memory({ initial: 1, address: "i64" }), TypeError, "Invalid argument type in ToBigInt operation");
    assert.throws(() => new WebAssembly.Memory({ initial: 1n, address: "i32" }), TypeError, "Conversion from 'BigInt' to 'number' is not allowed.");
}

// A table64 reports its length as a BigInt, matching grow()'s return value.
{
    const table = new WebAssembly.Table({ initial: 3n, maximum: 10n, element: "externref", address: "i64" });
    assert.eq(table.type().address, "i64");
    assert.eq(typeof table.length, "bigint");
    assert.eq(table.length, 3n);
    assert.eq(table.grow(1n), 3n);
    assert.eq(table.length, 4n);

    const table32 = new WebAssembly.Table({ initial: 3, element: "externref" });
    assert.eq(typeof table32.length, "number");
    assert.eq(table32.length, 3);
}

// A delta that cannot grow the table is a RangeError, whatever its magnitude.
{
    const table = new WebAssembly.Table({ initial: 1n, element: "externref", address: "i64" });
    assert.throws(() => table.grow(2n ** 33n), RangeError,
        "WebAssembly.Table.prototype.grow could not grow the table");
    // Outside the u64 range is a failed AddressValueToU64 instead, i.e. a TypeError.
    assert.throws(() => table.grow(2n ** 64n), TypeError, "Expect an integer argument in the range: [0, 2^64 - 1]");
    assert.throws(() => table.grow(-1n), TypeError, "Expect an integer argument in the range: [0, 2^64 - 1]");
}

// A memory64's declared maximum may exceed the memory32 page limit.
{
    const mem = new WebAssembly.Memory({ initial: 1n, maximum: 131072n, address: "i64" });
    assert.eq(mem.type().maximum, 131072n);
    assert.eq(new WebAssembly.Memory({ initial: 0n, maximum: 137438953471n, address: "i64" }).type().maximum, 137438953471n);
    assert.throws(() => new WebAssembly.Memory({ initial: 1n, maximum: 137438953472n, address: "i64" }), RangeError,
        "WebAssembly.Memory 'maximum' page count is too large");
    // An i32 memory keeps the memory32 limit.
    assert.throws(() => new WebAssembly.Memory({ initial: 1, maximum: 65537 }), RangeError,
        "WebAssembly.Memory 'maximum' page count is too large");
}
