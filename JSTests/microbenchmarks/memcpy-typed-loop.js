// Skipped under https://bugs.webkit.org/show_bug.cgi?id=202923
// due to timeout in ARMv7 that started between Oct 8 - Oct 10
//@ skip if $architecture == "arm"
//@ skip if $buildType == "debug"
function doTest(arr1, arr2) {
    if (arr1.length != arr2.length)
        return []
    for (let i=0; i<arr1.length; ++i) {
        arr2[i] = arr1[i]
    }
    return arr2
}
noInline(doTest);

let arr1 = new Int32Array(1000)
let arr2 = new Int32Array(1000)
for (let i=0; i<arr1.length; ++i) {
    arr1[i] = i
}

for (let i=0; i<10000000; ++i) doTest(arr1, arr2)

arr2 = new Int32Array(arr1.length)
doTest(arr1, arr2)

for (let i=0; i<arr1.length; ++i) {
    if (arr2[i] != arr1[i])
        throw "Error: bad copy"
}
