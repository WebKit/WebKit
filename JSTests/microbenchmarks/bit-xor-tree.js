//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
var result = 0;
for (var i = 0; i < 20000000; ++i) {
    result ^= i;
    result ^= i + 1;
    result ^= i + 2;
    result ^= i + 3;
    result ^= i + 4;
    result ^= i + 5;
    result ^= i + 6;
    result ^= i + 7;
}
if (result !== 0)
    throw "Error: bad result: " + result;
