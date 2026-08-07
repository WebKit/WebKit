function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: expected ${expected}, got ${actual}`);
}

// A canonical numeric index string outside the valid integer index range must be a no-op store, not a
// store at the index narrowed to the width of size_t. The low 32 bits of each key below are within the
// array's length, so a narrowing store would land on an element.
const keys = ["4294967295", "4294967296", "4294967299", "8589934592", "1e+21"];

for (const constructor of [Uint8Array, Int32Array, Float64Array, BigInt64Array]) {
    const zero = constructor === BigInt64Array ? 0n : 0;
    const value = constructor === BigInt64Array ? 42n : 42;

    for (const key of keys) {
        const array = new constructor(8);
        let coerced = 0;
        array[key] = { valueOf() { ++coerced; return value; } };

        shouldBe(coerced, 1); // TypedArraySetElement coerces the RHS before validating the index.
        shouldBe(array[key], undefined);
        shouldBe(Object.getOwnPropertyDescriptor(array, key), undefined);
        shouldBe(Object.prototype.hasOwnProperty.call(array, key), false);
        for (let i = 0; i < array.length; ++i)
            shouldBe(array[i], zero);
    }

    // The same keys through defineProperty, which must throw rather than store.
    for (const key of keys) {
        const array = new constructor(8);
        let threw = false;
        try {
            Object.defineProperty(array, key, { value });
        } catch (error) {
            threw = error instanceof TypeError;
        }
        shouldBe(threw, true);
        for (let i = 0; i < array.length; ++i)
            shouldBe(array[i], zero);
    }
}
