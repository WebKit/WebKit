//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
function doTest(arr1, arr2) {
    for (let i=0; i<arr1.length; ++i) {
        arr2[i] = arr1[i]
    }
    return arr2
}
noInline(doTest);

let arr1 = new Int32Array(10)
let arr2 = new Int32Array(10)
for (let i=0; i<arr1.length; ++i) {
    arr1[i] = i
}

for (let i=0; i<500000; ++i) doTest(arr1, arr2)

arr2 = new Int32Array(arr1.length)
doTest(arr1, arr2)

for (let i=0; i<arr1.length; ++i) {
    if (arr2[i] != arr1[i])
        throw "Error: bad copy: " + i + " " + arr1[i] + " " + arr2[i]
}
