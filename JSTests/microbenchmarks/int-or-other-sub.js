//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
var p = {f:true};

function foo(a, b) {
    var c;
    if (p.f)
        c = b;
    return a - c;
}

var result = 0;
for (var i = 0; i < 1000000; ++i)
    result += foo(i, i + 1);

if (result != -1000000)
    throw "Bad result: " + result;

