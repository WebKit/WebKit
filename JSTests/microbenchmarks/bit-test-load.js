//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
//@ $skipModes << :lockdown if $buildType == "debug"
let glob = 0
let arr = new Int32Array(8)

function doTest() {
    if ((arr[0]>>16)&1)
        glob += 1
    if ((arr[1]>>15)&1)
        glob -= 1
    if ((arr[2]>>18)&1)
        glob += 1
    if ((arr[3]>>19)&1)
        glob += 1
}
noInline(doTest);

for (let i=0; i<(1<<25); ++i) {
    arr[0] = arr[1] = arr[2] = arr[3] = i
    doTest()
}

if (glob != 33554432)
    throw "Error: bad result: " + glob;
