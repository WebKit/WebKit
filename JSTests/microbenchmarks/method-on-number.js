//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
//@ runNoFTL

function foo(a) {
    var result = 0;
    for (var i = 0 ; i < 500000; ++i)
        result += a.valueOf();
    return result;
}

var result = foo(5);

if (result != 2500000)
    throw "Bad result: " + result;
