"use strict";

function foo(a, b) {
    return a + b;
}

function bar() {
    return foo.apply(null, arguments);
}

noInline(bar);

for (var i = 0; i < 1000000; ++i) {
    var result = bar(1, 2);
    if (result != 3)
        throw "Error: bad result: " + result;
}

