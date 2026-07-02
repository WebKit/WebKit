"use strict"

// Test in-bounds access.
function opaqueGetByVal1(array, index) {
    return array[index];
}
noInline(opaqueGetByVal1);

function testAccessInBounds() {
    const target = new Array(100);

    // We start with an original array. Those GetByVal can be eliminated.
    for (let i = 0; i < 1e4; ++i) {
        const value = opaqueGetByVal1(target, i % 100);
        if (value !== undefined)
            throw "opaqueGetByVal1() case 1 failed for i = " + i + " value = " + value;
    }

    // Adding non-indexed properties to change the kind of array we are dealing with.
    target["webkit"] = "awesome!";
    target[-5] = "Uh?";
    for (let i = 0; i < 1e4; ++i) {
        const value = opaqueGetByVal1(target, i % 100);
        if (value !== undefined)
            throw "opaqueGetByVal1() case 2 failed for i = " + i + " value = " + value;
    }

    if (target["webkit"] !== "awesome!")
        throw "Failed to retrieve \"webkit\"";
    if (opaqueGetByVal1(target, -5) !== "Uh?")
        throw "Failed to retrive -5";
}
testAccessInBounds();

// Empty array access.
function opaqueGetByVal2(array, index) {
    return array[index];
}
noInline(opaqueGetByVal2);

function testEmptyArrayAccess() {
    const target = new Array();

    // We start with an original array. Those GetByVal can be eliminated.
    for (let i = 0; i < 1e4; ++i) {
        const value = opaqueGetByVal2(target, i % 100);
        if (value !== undefined)
            throw "opaqueGetByVal2() case 1 failed for i = " + i + " value = " + value;
    }

    // Adding non-indexed properties to change the kind of array we are dealing with.
    target["webkit"] = "awesome!";
    target[-5] = "Uh?";
    for (let i = 0; i < 1e4; ++i) {
        const value = opaqueGetByVal2(target, i % 100);
        if (value !== undefined)
            throw "opaqueGetByVal2() case 2 failed for i = " + i + " value = " + value;
    }
    if (target["webkit"] !== "awesome!")
        throw "Failed to retrieve \"webkit\"";
    if (opaqueGetByVal2(target, -5) !== "Uh?")
        throw "Failed to retrive -5";
}
testEmptyArrayAccess();

// Out of bounds array access.
function opaqueGetByVal3(array, index) {
    return array[index];
}
noInline(opaqueGetByVal3);

function testOutOfBoundsArrayAccess() {
    const target = new Array(42);

    // We start with an original array. Those GetByVal can be eliminated.
    for (let i = 0; i < 1e4; ++i) {
        const value = opaqueGetByVal3(target, i + 43);
        if (value !== undefined)
            throw "opaqueGetByVal3() case 1 failed for i = " + i + " value = " + value;
    }

    // Adding non-indexed properties to change the kind of array we are dealing with.
    target["webkit"] = "awesome!";
    target[-5] = "Uh?";
    for (let i = 0; i < 1e4; ++i) {
        const value = opaqueGetByVal3(target, i + 43);
        if (value !== undefined)
            throw "opaqueGetByVal3() case 2 failed for i = " + i + " value = " + value;
    }
    if (target["webkit"] !== "awesome!")
        throw "Failed to retrieve \"webkit\"";
    if (opaqueGetByVal3(target, -5) !== "Uh?")
        throw "Failed to retrive -5";
}
testOutOfBoundsArrayAccess();

// In-and-out of bounds.
function opaqueGetByVal4(array, index) {
    return array[index];
}
noInline(opaqueGetByVal4);

function testInAndOutOfBoundsArrayAccess() {
    const target = new Array(71);

    // We start with an original array. Those GetByVal can be eliminated.
    for (let i = 0; i < 1e4; ++i) {
        const value = opaqueGetByVal4(target, i);
        if (value !== undefined)
            throw "opaqueGetByVal4() case 1 failed for i = " + i + " value = " + value;
    }

    // Adding non-indexed properties to change the kind of array we are dealing with.
    target["webkit"] = "awesome!";
    target[-5] = "Uh?";
    for (let i = 0; i < 1e4; ++i) {
        const value = opaqueGetByVal4(target, i);
        if (value !== undefined)
            throw "opaqueGetByVal4() case 2 failed for i = " + i + " value = " + value;
    }
    if (target["webkit"] !== "awesome!")
        throw "Failed to retrieve \"webkit\"";
    if (opaqueGetByVal4(target, -5) !== "Uh?")
        throw "Failed to retrive -5";
}
testInAndOutOfBoundsArrayAccess();

// Negative index.
function opaqueGetByVal5(array, index) {
    return array[index];
}
noInline(opaqueGetByVal5);

function testNegativeIndex() {
    const target = new Array();

    // We start with an original array. Those GetByVal can be eliminated.
    for (let i = 0; i < 1e4; ++i) {
        const value = opaqueGetByVal5(target, -1 - i);
        if (value !== undefined)
            throw "opaqueGetByVal5() case 1 failed for i = " + i + " value = " + value;
    }

    // Adding non-indexed properties to change the kind of array we are dealing with.
    target["webkit"] = "awesome!";
    target[-5] = "Uh?";
    for (let i = 0; i < 1e4; ++i) {
        const value = opaqueGetByVal5(target, -1 - i);
        if (i === 4) {
            if (value !== "Uh?")
                throw "opaqueGetByVal5() case 2 failed for i = " + i + " value = " + value;
        } else if (value !== undefined)
            throw "opaqueGetByVal5() case 2 failed for i = " + i + " value = " + value;
    }
    if (target["webkit"] !== "awesome!")
        throw "Failed to retrieve \"webkit\"";
    if (opaqueGetByVal5(target, -5) !== "Uh?")
        throw "Failed to retrive -5";
}
testNegativeIndex();

// Test integer boundaries.
function opaqueGetByVal6(array, index) {
    return array[index];
}
noInline(opaqueGetByVal6);

function testIntegerBoundaries() {
    const target = new Array(42);

    for (let i = 0; i < 1e4; ++i) {
        // 2^31 - 1
        let value = opaqueGetByVal6(target, 2147483647);
        if (value !== undefined)
            throw "opaqueGetByVal6() case 1 failed for 2147483647 value = " + value;

        // 2^31
        value = opaqueGetByVal6(target, 2147483648);
        if (value !== undefined)
            throw "opaqueGetByVal6() case 1 failed for 2147483648 value = " + value;

        // 2^32 - 1
        value = opaqueGetByVal6(target, 4294967295);
        if (value !== undefined)
            throw "opaqueGetByVal6() case 1 failed for 4294967295 value = " + value;

        // 2^32
        value = opaqueGetByVal6(target, 4294967296);
        if (value !== undefined)
            throw "opaqueGetByVal6() case 1 failed for 4294967296 value = " + value;

        // 2^52
        value = opaqueGetByVal6(target, 4503599627370496);
        if (value !== undefined)
            throw "opaqueGetByVal6() case 1 failed for 4503599627370496 value = " + value;
    }
}
testIntegerBoundaries();

// Use a constant index.
function opaqueGetByVal7_zero(array) {
    return array[0];
}
noInline(opaqueGetByVal7_zero);

function opaqueGetByVal7_doubleZero(array) {
    return array[1.5 - 1.5];
}
noInline(opaqueGetByVal7_doubleZero);

function opaqueGetByVal7_101(array) {
    return array[101];
}
noInline(opaqueGetByVal7_101);

function opaqueGetByVal7_double101(array) {
    return array[101.0000];
}
noInline(opaqueGetByVal7_double101);

function opaqueGetByVal7_1038(array) {
    return array[1038];
}
noInline(opaqueGetByVal7_1038);

function testContantIndex() {
    const emptyArray = new Array();

    for (let i = 0; i < 1e4; ++i) {
        let value = opaqueGetByVal7_zero(emptyArray);
        if (value !== undefined)
            throw "opaqueGetByVal7() case 1 failed for 0 value = " + value;

        value = opaqueGetByVal7_doubleZero(emptyArray);
        if (value !== undefined)
            throw "opaqueGetByVal7_doubleZero() case 1 failed for 0 value = " + value;

        value = opaqueGetByVal7_101(emptyArray);
        if (value !== undefined)
            throw "opaqueGetByVal7() case 1 failed for 101 value = " + value;

        value = opaqueGetByVal7_double101(emptyArray);
        if (value !== undefined)
            throw "opaqueGetByVal7() case 1 failed for 101 value = " + value;
    }

    const uninitializedArray = new Array(1038);

    for (let i = 0; i < 1e4; ++i) {
        let value = opaqueGetByVal7_zero(uninitializedArray);
        if (value !== undefined)
            throw "opaqueGetByVal7() case 2 failed for 0 value = " + value;

        value = opaqueGetByVal7_doubleZero(uninitializedArray);
        if (value !== undefined)
            throw "opaqueGetByVal7_doubleZero() case 2 failed for 0 value = " + value;

        value = opaqueGetByVal7_101(uninitializedArray);
        if (value !== undefined)
            throw "opaqueGetByVal7() case 2 failed for 101 value = " + value;

        value = opaqueGetByVal7_double101(uninitializedArray);
        if (value !== undefined)
            throw "opaqueGetByVal7_double101() case 2 failed for 101 value = " + value;

        value = opaqueGetByVal7_1038(uninitializedArray);
        if (value !== undefined)
            throw "opaqueGetByVal7() case 2 failed for 1038 value = " + value;
    }

}
testContantIndex();

// Test natural integer proggression.
function testValueIsUndefinedInNaturalProgression(value) {
    if (value !== undefined)
        throw "Invalid value in natural progression test"
}
noInline(testValueIsUndefinedInNaturalProgression);

function testNaturalProgression() {
    const target = new Array(42);

    for (let i = 0; i < 10; ++i) {
        const value = target[i];
        testValueIsUndefinedInNaturalProgression(value);
    }

    const emptyTarget = new Array();
    for (let i = 10; i--;) {
        const value = emptyTarget[i];
        testValueIsUndefinedInNaturalProgression(value);
    }
}
noInline(testNaturalProgression);
for (let i = 0; i < 1e4; ++i)
    testNaturalProgression();

// PutByVal changes the array type.
function getUndecidedArray()
{
    return new Array(50);
}
noInline(getUndecidedArray);

for (let i = 0; i < 1e4; ++i) {
    // Warm up getUndecidedArray() without any useful profiling information.
    getUndecidedArray();
}

function getByValAfterPutByVal()
{
    const array = getUndecidedArray();

    for (let i = 0; i < array.length + 1; ++i) {
        if (array[i] !== undefined)
            throw "Invalid access on the empty array in getByValAfterPutByVal()";
    }

    array[5] = 42;

    for (let i = array.length + 1; i--;) {
        if (i === 5) {
            if (array[i] !== 42)
                throw "array[5] !== 42"
        } else if (array[i] !== undefined)
            throw "Invalid access on the mostly empty array in getByValAfterPutByVal()";
    }
}
noInline(getByValAfterPutByVal);

for (let i = 0; i < 1e4; ++i)
    getByValAfterPutByVal();

// Push changes the array type.
function getByValAfterPush()
{
    const array = getUndecidedArray();

    for (let i = 0; i < array.length + 1; ++i) {
        if (array[i] !== undefined)
            throw "Invalid access on the empty array in getByValAfterPush()";
    }

    array.push(43);

    for (let i = array.length + 1; i--;) {
        if (i === 50) {
            if (array[i] !== 43)
                throw "array[50] !== 43"
        } else if (array[i] !== undefined)
            throw "Invalid access on the mostly empty array in getByValAfterPush()";
    }
}
noInline(getByValAfterPutByVal);

for (let i = 0; i < 1e4; ++i)
    getByValAfterPush();
