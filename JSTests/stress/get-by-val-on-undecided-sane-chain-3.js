"use strict"

// Test in-bounds access.
function opaqueGetByVal1(array, index) {
    return array[index];
}
noInline(opaqueGetByVal1);

function testUninitializedArray() {
    const target = new Array(100);

    // We start with an original array. Those GetByVal can be eliminated.
    for (let i = 0; i < 1e4; ++i) {
        const value = opaqueGetByVal1(target, i);
        if (value !== undefined)
            throw "opaqueGetByVal1() case 1 failed for i = " + i + " value = " + value;
    }

    Array.prototype[-1] = "Uh!";

    for (let i = 0; i < 1e4; ++i) {
        const value = opaqueGetByVal1(target, i);
        if (value !== undefined)
            throw "opaqueGetByVal1() case 2 failed for i = " + i + " value = " + value;
    }
    const prototypeValue = opaqueGetByVal1(target, -1)
    if (prototypeValue !== "Uh!")
        throw "prototypeValue value = " + value;

}
testUninitializedArray();

// Test in-bounds access.
function opaqueGetByVal2(array, index) {
    return array[index];
}
noInline(opaqueGetByVal2);

function testAccessOnEmpty() {
    const target = new Array();

    for (let i = 0; i < 1e4; ++i) {
        const value = opaqueGetByVal2(target, i);
        if (value !== undefined)
            throw "opaqueGetByVal2() case 1 failed for i = " + i + " value = " + value;
    }
    const prototypeValue = opaqueGetByVal2(target, -1)
    if (prototypeValue !== "Uh!")
        throw "prototypeValue value = " + value;
}
testAccessOnEmpty();
