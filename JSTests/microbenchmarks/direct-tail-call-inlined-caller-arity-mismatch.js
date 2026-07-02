//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
"use strict";

function foo(a, b, c, d, e, f, g)
{
    return 42;
}

function bar()
{
    return foo();
}

function baz()
{
    return bar() + 1;
}

noInline(foo);
noInline(baz);

(function() {
    for (var i = 0; i < 10000000; ++i) {
        var result = baz();
        if (result != 43)
            throw "Error: bad result: " + result;
    }
})();

