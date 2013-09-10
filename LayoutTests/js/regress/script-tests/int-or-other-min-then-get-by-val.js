var p = {f:true};

function foo(a, b, d) {
    var c;
    if (p.f)
        c = b;
    return d[Math.min(a, c)];
}

var result = 0;
var array = [42, 43, 44];
for (var i = 0; i < 400000; ++i)
    result += foo((i % 3) * 2, (i % 2) + 1, array);

if (result != 17199999)
    throw "Bad result: " + result;

