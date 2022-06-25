//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
function doTest(arr1) {
    let sum = 0
    for (let i=0; i<arr1.length; ++i) {
        sum += arr1[i]
    }
    return sum
}
noInline(doTest);

let arr1 = new Int32Array(1000)

for (let i=0; i<1000; ++i) doTest(arr1)
