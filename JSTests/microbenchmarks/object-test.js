//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
function foo(a) {
    return typeof(a) == "object";
}

var array = ["string", 5, 6.5, void(0), null, true, false, {f:42}, [1, 2, 3]];

var result = 0;
for (var i = 0; i < 100000; ++i) {
    result *= 3;
    result += foo(array[i % array.length]) | 0;
    result |= 0;
}

if (result != 719228699)
    throw "Bad result: " + result;
