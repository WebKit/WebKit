"use strict";
function foo(x, y)
{
    if (arguments.length >= 2)
        return foo(x+y)
    return x;
}
noInline(foo);

function bar(x)
{
    if (arguments.length >= 2)
        return bar(arguments[0] + arguments[1])
    return x;
}
noInline(bar);

for (var i = 0; i < 10000; ++i) {
    var result = foo(40, 2);
    if (result !== 42)
        throw "Wrong result for foo, expected 42, got " + result;
    result = bar(40, 2);
    if (result !== 42)
        throw "Wrong result for bar, expected 42, got " + result;
}
