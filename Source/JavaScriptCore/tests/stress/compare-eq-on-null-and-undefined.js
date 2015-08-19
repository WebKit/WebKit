"use strict"

// Trivial cases: everything is monomorphic and super predictable.
function compareConstants()
{
    return (null == null) && (null == undefined) && (undefined == null);
}
noInline(compareConstants);

for (let i = 0; i < 1e4; ++i) {
    if (!compareConstants())
        throw "Failed to compareConstants().";
}


function opaqueNull() {
    return null;
}
noInline(opaqueNull);

function opaqueUndefined() {
    return undefined;
}
noInline(opaqueUndefined);

function compareConstantsAndDynamicValues()
{
    return ((null == opaqueNull())
        && (opaqueNull() == null)
        && (undefined == opaqueNull())
        && (opaqueNull() == undefined)
        && (null == opaqueUndefined())
        && (opaqueUndefined() == null)
        && (undefined == opaqueUndefined())
        && (opaqueUndefined() == undefined));
}
noInline(compareConstantsAndDynamicValues);

for (let i = 1e4; i--;) {
    if (!compareConstantsAndDynamicValues())
        throw "Failed compareConstantsAndDynamicValues()";
}


function compareDynamicValues()
{
    return ((opaqueNull() == opaqueNull())
            && (opaqueUndefined() == opaqueUndefined())
            && (opaqueNull() == opaqueUndefined())
            && (opaqueUndefined() == opaqueNull()));
}
noInline(compareDynamicValues);

for (let i = 0; i < 1e4; ++i) {
    if (!compareDynamicValues())
        throw "Failed compareDynamicValues()";
}


function compareDynamicValueToItself()
{
    const value1 = opaqueNull();
    const value2 = opaqueUndefined();
    return value1 == value1 && value2 == value2;
}
noInline(compareDynamicValueToItself);

for (let i = 0; i < 1e4; ++i) {
    if (!compareDynamicValueToItself())
        throw "Failed compareDynamicValueToItself()";
}


// The case that interested us in the first place.
// Accessing an array with undecided shape always return undefined.

function arrayTesting()
{
    let returnValue = true;

    const array1 = new Array(2);
    for (let i = 0; i < 3; ++i) {
        returnValue = returnValue && (array1[i] == null);
        returnValue = returnValue && (null == array1[i]);
        returnValue = returnValue && (array1[i] == undefined);
        returnValue = returnValue && (undefined == array1[i]);
    }

    const array2 = new Array(2);
    for (let i = 0; i < 2; ++i) {
        returnValue = returnValue && (array2[i] == opaqueNull());
        returnValue = returnValue && (opaqueNull() == array2[i]);
        returnValue = returnValue && (array2[i] == opaqueUndefined());
        returnValue = returnValue && (opaqueUndefined() == array2[i]);
    }

    const array3 = new Array(2);
    for (let i = 0; i < 3; ++i) {
        returnValue = returnValue && (array3[i] == array3[i]);
        returnValue = returnValue && (array1[i] == array3[i]);
        returnValue = returnValue && (array3[i] == array1[i]);
        returnValue = returnValue && (array2[i] == array3[i]);
        returnValue = returnValue && (array3[i] == array2[i]);

    }

    return returnValue;
}
noInline(arrayTesting);

for (let i = 0; i < 1e4; ++i) {
    if (!arrayTesting())
        throw "Failed arrayTesting()";
}


// Let's make it polymorphic after optimization. We should fallback to a generic compare operation.

function opaqueCompare1(a, b) {
    return a == b;
}
noInline(opaqueCompare1);

function testNullComparatorUpdate() {
    for (let i = 0; i < 1e4; ++i) {
        if (!opaqueCompare1(null, null))
            throw "Failed opaqueCompare1(null, null)"
    }

    // Let's change types
    for (let i = 0; i < 1e4; ++i) {
        if (opaqueCompare1("foo", null))
            throw "Failed opaqueCompare1(\"foo\", null)"
    }
}
testNullComparatorUpdate();

function opaqueCompare2(a, b) {
    return a == b;
}
noInline(opaqueCompare2);

function testUndefinedComparatorUpdate() {
    for (let i = 0; i < 1e4; ++i) {
        if (!opaqueCompare2(undefined, undefined))
            throw "Failed opaqueCompare2(undefined, undefined)"
    }

    // Let's change types
    for (let i = 0; i < 1e4; ++i) {
        if (!opaqueCompare2("bar", "bar"))
            throw "Failed opaqueCompare2(\"bar\", \"bar\")"
    }
}
testUndefinedComparatorUpdate();

function opaqueCompare3(a, b) {
    return a == b;
}
noInline(opaqueCompare3);

function testNullAndUndefinedComparatorUpdate() {
    for (let i = 0; i < 1e4; ++i) {
        if (!opaqueCompare3(undefined, null) || !opaqueCompare2(null, undefined))
            throw "Failed opaqueCompare2(undefined/null, undefined/null)"
    }

    // Let's change types
    for (let i = 0; i < 1e4; ++i) {
        if (opaqueCompare3(undefined, "bar"))
            throw "Failed opaqueCompare3(undefined, \"bar\")"
    }
}
testNullAndUndefinedComparatorUpdate();
