//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
function foo(string) {
    var result;
    for (var i = 0; i < 1000000; ++i)
        result = string[0];
    return result;
}

var result = foo("x");
if (result != "x")
    throw "Bad result: " + result;
