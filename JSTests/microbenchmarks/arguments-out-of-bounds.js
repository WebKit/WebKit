//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
function foo() { return arguments; }
noInline(foo);

function bar(o) {
    var tmp = o[0];
    var result = 0;
    for (var i = 0; i < 1000; ++i) {
        if (tmp)
            result += tmp * i;
    }
    return result;
}
noInline(bar);

var result = 0;
var o = foo();
for (var i = 0; i < 10000; ++i)
    result += bar(o);

if (result !== 0)
    throw "Error: bad result: " + result;
