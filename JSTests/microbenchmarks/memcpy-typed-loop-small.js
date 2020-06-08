//@ skip if not $jitTests
function doTest(arr1, arr2) {
    if (arr1.length != arr2.length)
        return []
    for (let i=0; i<arr1.length; ++i) {
        arr2[i] = arr1[i]
    }
    return arr2
}
noInline(doTest);

for (size of [1, 3, 8, 10, 12, 16]) {
    let arr1 = new Int32Array(size)
    let arr2 = new Int32Array(size)
    for (let i=0; i<arr1.length; ++i) {
        arr1[i] = i
    }

    for (let i=0; i<50000000; ++i) doTest(arr1, arr2)

    arr2 = new Int32Array(arr1.length)
    doTest(arr1, arr2)

    for (let i=0; i<arr1.length; ++i) {
        if (arr2[i] != arr1[i])
            throw "Error: bad copy: " + i + " " + arr1[i] + " " + arr2[i]
    }
}
