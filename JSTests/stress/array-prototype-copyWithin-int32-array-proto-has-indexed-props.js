function compareArray(actual, expected) {
    if (actual.length !== expected.length)
        throw new Error(`Expected length ${expected.length} but got ${actual.length}`);
    for (let i = 0, l = actual.length; i < l; ++i) {
        if (Object.hasOwn(actual, i) !== Object.hasOwn(expected, i))
            throw new Error(`${i}: Mismatch the owner of the property`);
        if (actual[i] !== expected[i])
            throw new Error(`${i}: Expected ${expected[i]} but got ${actual[i]}`);
    }
}

Array.prototype[2] = 2;

{
    const arr = [0, 1, , 3];
    arr.copyWithin(0, 2)
    compareArray(arr, [2, 3, , 3]);
}
