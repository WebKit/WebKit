//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
function doTest(max) {
    let sum = 0
    for (let i=0; i<max; ++i) {
        sum = sum + i * i;
    }
    return sum
}
noInline(doTest);

for (let i=0; i<100000; ++i) doTest(10000)

if (doTest(10000) != 333283335000)
    throw "Error: bad result: " + doTest(10000);
