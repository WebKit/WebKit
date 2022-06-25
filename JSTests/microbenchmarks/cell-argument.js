//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
function foo(o) {
    var result = 0;
    for (var i = 0; i < 5000; ++i)
        result += o.f;
    return result;
}

var o = {f:42};
var result = 0;
for (var i = 0; i < 1000; ++i)
    result += foo(o);

if (result != 210000000)
    throw "Error: bad result: " + result;

