"use strict";

function foo(a, b, c, d, e, f) {
    return arguments.length;
}

noInline(foo);

function bar() {
    return foo(1, 2, 3);
}

noInline(bar);

for (var i = 0; i < 10000; ++i) {
    var result = bar();
    if (result != 3)
        throw "Error: bad result: " + result;
}

