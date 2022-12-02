//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
//@ $skipModes << :lockdown if $buildType == "debug"

function doTest(arr1) {
    let arr2 = []
    for (let i=0; i<arr1.length; ++i) {
        arr2[i] = arr1[i]
    }
    return arr2
}
noInline(doTest);

let arr1 = []
for (let i=0; i<1000; ++i) {
    arr1[i] = { test: i }
}

for (let i=0; i<100000; ++i) doTest(arr1)

if (doTest(arr1) == arr1)
    throw "Error: did not copy"

if (doTest(arr1)[5].test != 5)
    throw "Error: bad copy"

doTest(arr1)[5].test = 42

if (arr1[5].test != 42)
    throw "Error: bad copy"
