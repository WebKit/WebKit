// A canonical numeric index string on a typed array never reaches ordinary property lookup, whatever
// number it denotes. https://tc39.es/ecma262/#sec-canonicalnumericindexstring
//
// Keys past MAX_ARRAY_INDEX are recognized as integer indices, so they must behave like any other
// out-of-bounds index on a short array rather than becoming ordinary properties.

function shouldBe(actual, expected, what) {
    if (actual !== expected)
        throw new Error(`bad value for ${what}: expected ${expected} but got ${actual}`);
}

function shouldThrowTypeError(f, what) {
    try {
        f();
    } catch (e) {
        if (e instanceof TypeError)
            return;
        throw new Error(`${what}: expected a TypeError but got ${e}`);
    }
    throw new Error(`${what}: expected a TypeError but nothing was thrown`);
}

const array = new Uint8Array(4);
const prototype = Object.getPrototypeOf(array);

// String(n) is by construction the canonical spelling of n, so each of these is a
// CanonicalNumericIndexString. They span both sides of every internal cutoff: MAX_ARRAY_INDEX, 2^32,
// 2^53, and 2^64, plus the values that are canonical but are not integer indices at all.
const keys = [
    2 ** 32 - 1,        // 0xFFFFFFFF, excluded by isIndex()
    2 ** 32,
    2 ** 53 - 1,        // the largest integer index the spec allows
    2 ** 53,
    2 ** 60,
    2 ** 64,            // past uint64_t, so no index is recoverable
    1e21,               // spelled in exponential form
    -1,
    1.5,
    2 ** 32 + 0.5,      // canonical, past MAX_ARRAY_INDEX, and not an integer
    NaN,
    Infinity,
    -Infinity,
].map(String).concat(["-0"]); // String(-0) is "0", so -0 has to be spelled out.

for (const key of keys) {
    // An element that does not exist must not be found on the prototype either.
    prototype[key] = "fromPrototype";

    shouldBe(array[key], undefined, `array["${key}"]`);
    shouldBe(key in array, false, `"${key}" in array`);
    shouldBe(array.hasOwnProperty(key), false, `hasOwnProperty("${key}")`);
    shouldBe(Object.getOwnPropertyDescriptor(array, key), undefined, `getOwnPropertyDescriptor("${key}")`);

    // The store is ignored, but the right hand side is still coerced first.
    let coerced = false;
    array[key] = { valueOf() { coerced = true; return 1; } };
    shouldBe(coerced, true, `array["${key}"] = ... coerces the right hand side`);
    shouldBe(array.hasOwnProperty(key), false, `"${key}" was not added as a property`);

    // Deleting an index that does not exist succeeds vacuously.
    shouldBe(delete array[key], true, `delete array["${key}"]`);

    shouldThrowTypeError(() => Object.defineProperty(array, key, { value: 1 }), `defineProperty("${key}")`);

    delete prototype[key];
}

// Numeric-looking strings that are not canonical are ordinary properties, and stay that way.
for (const key of ["042", "1e3", " 1", "0x10", "4294967296 ", "+4294967296"]) {
    array[key] = "ordinary";
    shouldBe(array.hasOwnProperty(key), true, `"${key}" is an ordinary property`);
    shouldBe(array[key], "ordinary", `array["${key}"]`);
    shouldBe(delete array[key], true, `delete array["${key}"]`);
}
