"use strict";

function foo(a, b, c, d, e, f, g)
{
    return 42;
}

function bar()
{
    return foo();
}

noInline(foo);
noInline(bar);

(function() {
    for (var i = 0; i < 10000000; ++i) {
        var result = bar();
        if (result != 42)
            throw "Error: bad result: " + result;
    }
})();

