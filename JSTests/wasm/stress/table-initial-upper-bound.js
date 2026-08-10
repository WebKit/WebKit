import * as assert from "../assert.js";

// https://www.w3.org/TR/wasm-js-api-2/#limits says the maximum size of a table
// is 10,000,000, so that size itself is creatable and only larger ones throw.
const maxTableEntries = 10000000;

for (const element of ["externref", "anyfunc"]) {
    for (const address of [undefined, "i32", "i64"]) {
        const is64 = address === "i64";
        const size = value => is64 ? BigInt(value) : value;
        const descriptor = value => {
            const result = { element, initial: size(value) };
            if (address !== undefined)
                result.address = address;
            return result;
        };

        const message = `WebAssembly.Table 'initial' value is above the upper bound ${maxTableEntries}`;
        assert.throws(() => new WebAssembly.Table(descriptor(maxTableEntries + 1)), RangeError, message);

        // A declared maximum is not bounded by what can be allocated, so only
        // the initial size is rejected here.
        const table = new WebAssembly.Table({ ...descriptor(1), maximum: size(maxTableEntries + 1) });
        assert.eq(table.length, size(1));
    }
}

// The bound itself is accepted, both at creation and by growth. Only externref
// is exercised: a funcref table of this size also allocates an inline function
// table.
const table = new WebAssembly.Table({ element: "externref", initial: maxTableEntries });
assert.eq(table.length, maxTableEntries);

const grown = new WebAssembly.Table({ element: "externref", initial: 1 });
assert.eq(grown.grow(maxTableEntries - 1), 1);
assert.eq(grown.length, maxTableEntries);
assert.throws(() => grown.grow(1), RangeError, "WebAssembly.Table.prototype.grow could not grow the table");
