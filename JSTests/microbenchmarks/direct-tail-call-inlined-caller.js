"use strict";

function foo()
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

