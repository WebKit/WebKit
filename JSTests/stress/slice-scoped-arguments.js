function shouldBe(a, b) {
    if (a !== b)
        throw new Error(`Expected ${b} but got ${a}`);
}

function shouldBeArray(a, b) {
    shouldBe(a.length, b.length);
    for (let i = 0; i < a.length; ++i)
        shouldBe(a[i], b[i]);
}

// Test basic slice on ScopedArguments
function testSlice(x) {
    // Reference x to make arguments a ScopedArguments
    (function () { return x; })();
    return Array.prototype.slice.call(arguments);
}
noInline(testSlice);

function testSliceWithStart(x) {
    (function () { return x; })();
    return Array.prototype.slice.call(arguments, 1);
}
noInline(testSliceWithStart);

function testSliceWithStartEnd(x) {
    (function () { return x; })();
    return Array.prototype.slice.call(arguments, 1, 3);
}
noInline(testSliceWithStartEnd);

// Test splice on ScopedArguments
function testSplice(x) {
    (function () { return x; })();
    return Array.prototype.splice.call(arguments, 1, 2);
}
noInline(testSplice);

// Warm up
for (let i = 0; i < 1e4; ++i) {
    // contiguous
    {
        const value1 = { value: 1 };
        const value2 = { value: 2 };
        const value3 = { value: 3 };
        const value4 = { value: 4 };
        const value5 = { value: 5 };

        const array = testSlice(value1, value2, value3, value4, value5);
        shouldBe(array.length, 5);
        shouldBe(array[0], value1);
        shouldBe(array[1], value2);
        shouldBe(array[2], value3);
        shouldBe(array[3], value4);
        shouldBe(array[4], value5);

        const array2 = testSliceWithStart(value1, value2, value3, value4, value5);
        shouldBe(array2.length, 4);
        shouldBe(array2[0], value2);
        shouldBe(array2[1], value3);
        shouldBe(array2[2], value4);
        shouldBe(array2[3], value5);

        const array3 = testSliceWithStartEnd(value1, value2, value3, value4, value5);
        shouldBe(array3.length, 2);
        shouldBe(array3[0], value2);
        shouldBe(array3[1], value3);

        const spliced = testSplice(value1, value2, value3, value4, value5);
        shouldBe(spliced.length, 2);
        shouldBe(spliced[0], value2);
        shouldBe(spliced[1], value3);
    }

    // double
    {
        const array = testSlice(1.1, 2.1, 3.1, 4.1, 5.1);
        shouldBe(array.length, 5);
        shouldBe(array[0], 1.1);
        shouldBe(array[1], 2.1);
        shouldBe(array[2], 3.1);
        shouldBe(array[3], 4.1);
        shouldBe(array[4], 5.1);

        const array2 = testSliceWithStart(1.1, 2.1, 3.1, 4.1, 5.1);
        shouldBe(array2.length, 4);
        shouldBe(array2[0], 2.1);
        shouldBe(array2[1], 3.1);
        shouldBe(array2[2], 4.1);
        shouldBe(array2[3], 5.1);

        const array3 = testSliceWithStartEnd(1.1, 2.1, 3.1, 4.1, 5.1);
        shouldBe(array3.length, 2);
        shouldBe(array3[0], 2.1);
        shouldBe(array3[1], 3.1);
    }

    // int32
    {
        const array = testSlice(1, 2, 3, 4, 5);
        shouldBe(array.length, 5);
        shouldBe(array[0], 1);
        shouldBe(array[1], 2);
        shouldBe(array[2], 3);
        shouldBe(array[3], 4);
        shouldBe(array[4], 5);

        const array2 = testSliceWithStart(1, 2, 3, 4, 5);
        shouldBe(array2.length, 4);
        shouldBe(array2[0], 2);
        shouldBe(array2[1], 3);
        shouldBe(array2[2], 4);
        shouldBe(array2[3], 5);

        const array3 = testSliceWithStartEnd(1, 2, 3, 4, 5);
        shouldBe(array3.length, 2);
        shouldBe(array3[0], 2);
        shouldBe(array3[1], 3);
    }

    // empty slice
    {
        const array = testSliceWithStartEnd(1, 2, 3, 4, 5);
        // Actually this slices from index 1 to 3, so length is 2
    }
}

// Test edge cases

// Test with overflow arguments (more arguments than named parameters)
function testOverflow(a, b) {
    (function () { return a + b; })();
    return Array.prototype.slice.call(arguments, 2);
}
noInline(testOverflow);

for (let i = 0; i < 1e4; ++i) {
    const result = testOverflow(1, 2, 3, 4, 5);
    shouldBeArray(result, [3, 4, 5]);
}

// Test slicing only from overflow portion
function testOverflowOnly(a) {
    (function () { return a; })();
    return Array.prototype.slice.call(arguments, 1, 4);
}
noInline(testOverflowOnly);

for (let i = 0; i < 1e4; ++i) {
    const result = testOverflowOnly(1, 2, 3, 4, 5);
    shouldBeArray(result, [2, 3, 4]);
}

// Test slicing across named and overflow boundary
function testAcrossBoundary(a, b) {
    (function () { return a + b; })();
    return Array.prototype.slice.call(arguments, 1, 4);
}
noInline(testAcrossBoundary);

for (let i = 0; i < 1e4; ++i) {
    const result = testAcrossBoundary(1, 2, 3, 4, 5);
    shouldBeArray(result, [2, 3, 4]);
}

// Test empty result
function testEmptySlice(x) {
    (function () { return x; })();
    return Array.prototype.slice.call(arguments, 5, 5);
}
noInline(testEmptySlice);

for (let i = 0; i < 1e4; ++i) {
    const result = testEmptySlice(1, 2, 3, 4, 5);
    shouldBe(result.length, 0);
}

// Test negative indices
function testNegativeIndices(x) {
    (function () { return x; })();
    return Array.prototype.slice.call(arguments, -3, -1);
}
noInline(testNegativeIndices);

for (let i = 0; i < 1e4; ++i) {
    const result = testNegativeIndices(1, 2, 3, 4, 5);
    shouldBeArray(result, [3, 4]);
}
