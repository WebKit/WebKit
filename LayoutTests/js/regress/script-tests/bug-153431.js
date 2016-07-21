//@ runNoFTL

// Regression test for https://bugs.webkit.org/show_bug.cgi?id=153431.
// Reduced version based on the reproduction case provided by Ryan Sturgell in the bug,
// with some variable renames to read slightly better.

function assert(testedValue) {
    if (!testedValue)
        throw Error("Failed assertion");
}

function badFunc(arr, operand, resultArr) { 
    // This re-use of variable "operand" is important - rename it and the bug goes away.
    operand = arr[operand];
    if (false) {
        // If this unreachable block is removed, the bug goes away!!
    } else 
    {
        resultArr[0] = operand;
    }
}
noInline(badFunc);

function run() {
    for (var i = 0; i < 10000; i++) {
        var arr = new Uint32Array([0x80000000,1]); // Needs to be an Uint32Array.
        var resultArr = [];

        badFunc(arr, 0, resultArr);
        assert(resultArr[0] == arr[0]);
        badFunc(arr, 1, resultArr);
        assert(resultArr[0] == arr[1]);
    }
}

run();
