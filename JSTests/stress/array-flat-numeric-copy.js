function check(source, expected, depth = 1) {
    const first = source[0];
    const result = source.flat(depth);
    if (result === source || result.length !== expected.length)
        throw new Error("Unexpected result identity or length");
    for (let i = 0; i < expected.length; ++i) {
        if (!Object.hasOwn(result, i) || !Object.is(result[i], expected[i]))
            throw new Error("Unexpected result at " + i);
    }

    // Exercise the unused tail and writes to the newly allocated butterfly.
    result.push(42);
    if (result.length !== expected.length + 1 || result[expected.length] !== 42)
        throw new Error("Unexpected push result");
    result.length += 3;
    for (let i = expected.length + 1; i < result.length; ++i) {
        if (Object.hasOwn(result, i))
            throw new Error("Expected a hole in the extended result");
    }
    result[0] = -999.5;
    if (!Object.is(source[0], first))
        throw new Error("Result aliases source");
}
noInline(check);

// Check short copies, vector boundaries, and holes at each end and the middle.
for (const length of [0, 1, 2, 3, 4, 7, 8, 9, 15, 16, 17, 31, 32, 33, 63, 64, 65, 127, 128, 129]) {
    for (const fraction of [0, 0.5]) {
        const source = [];
        const expected = [];
        for (let i = 0; i < length; ++i) {
            source.push(i + fraction);
            expected.push(i + fraction);
        }
        check(source, expected);
        check(source, expected, 0);
        if (!length)
            continue;
        for (const index of [0, length >> 1, length - 1]) {
            const holey = source.slice();
            delete holey[index];
            const compacted = expected.slice();
            compacted.splice(index, 1);
            check(holey, compacted);
        }
    }
}

for (let i = 0; i < testLoopCount; ++i) {
    // Literals can use COW storage; push creates writable numeric storage.
    check([0, -1, -2147483648, 2147483647], [0, -1, -2147483648, 2147483647]);
    check([0.5, -0, Infinity, -Infinity], [0.5, -0, Infinity, -Infinity]);
    const ints = [];
    ints.push(i, i + 1, i + 2);
    check(ints, [i, i + 1, i + 2]);
    const doubles = [];
    doubles.push(i + 0.5, -0, Infinity);
    check(doubles, [i + 0.5, -0, Infinity]);
}

// NaN is a value, whereas missing properties must be removed.
check([1.5, NaN, , -0], [1.5, NaN, -0]);
check([, ,], []);

// A prototype's indexed value must not be discarded as a hole.
for (const fraction of [0, 0.5]) {
    const source = [1 + fraction, , 3 + fraction];
    const prototype = Object.create(Array.prototype);
    prototype[1] = 2 + fraction;
    Object.setPrototypeOf(source, prototype);
    check(source, [1 + fraction, 2 + fraction, 3 + fraction]);
}

// Depth conversion can change the source after its length has been read.
const source = [1.5, 2.5, 3.5];
check(source, [1.5, 3.5], { valueOf() { delete source[1]; return 1; } });

// A numeric input must still honor a custom species constructor.
class Result extends Array { }
const numeric = [1.5, 2.5];
numeric.constructor = { [Symbol.species]: Result };
const result = numeric.flat();
if (!(result instanceof Result) || result.length !== 2 || result[0] !== 1.5 || result[1] !== 2.5)
    throw new Error("Unexpected species result");
