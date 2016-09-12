function forceTransition() {
    // We want to test the StructureCheck in testSparseArray(), not this watchpoint.
    // We start with the transition so that it's nothing new.
    let array = new Array();
    array[100001] = "WebKit!";
}
forceTransition();

function opaqueGetArrayLength(array)
{
    return array.length;
}
noInline(opaqueGetArrayLength);

function testEmptyArray()
{
    let array = [];
    for (let i = 0; i < 1e6; ++i) {
        if (opaqueGetArrayLength(array) !== 0) {
            throw "Failed testEmptyArray";
        }
    }

    array = new Array();
    for (let i = 0; i < 1e6; ++i) {
        if (opaqueGetArrayLength(array) !== 0) {
            throw "Failed testEmptyArray";
        }
    }
}
testEmptyArray();


function testUnitializedArray()
{
    let array = new Array(32);
    for (let i = 0; i < 1e6; ++i) {
        if (opaqueGetArrayLength(array) !== 32) {
            throw "Failed testUnitializedArray";
        }
    }

    array = new Array();
    array.length = 64
    for (let i = 0; i < 1e6; ++i) {
        if (opaqueGetArrayLength(array) !== 64) {
            throw "Failed testUnitializedArray";
        }
    }
}
testUnitializedArray();

function testOversizedArray()
{
    let array = new Array(100001);
    for (let i = 0; i < 1e6; ++i) {
        if (opaqueGetArrayLength(array) !== 100001) {
            throw "Failed testOversizedArray";
        }
    }
}
testOversizedArray();

// This should OSR Exit and fallback to GetById to get the length.
function testSparseArray()
{
    let array = new Array();
    array[100001] = "WebKit!";
    for (let i = 0; i < 1e6; ++i) {
        if (opaqueGetArrayLength(array) !== 100002) {
            throw "Failed testOversizedArray";
        }
    }
}
testSparseArray();

