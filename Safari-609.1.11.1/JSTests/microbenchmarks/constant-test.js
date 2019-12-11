//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
function foo(a, b, c) {
    return a * b / c + a - b * c / a % b + c;
}

var result = 0;

for (var i = 0; i < 1000000; ++i)
    result += foo(1.5, 2.4, 3.3);

if (result != 5410909.091033336)
    throw "Bad result: " + result;


