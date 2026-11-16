"use strict";

function makeClosure(x)
{
    return (f) => {
        if (x == 42) {
            x = 0;
            return f(f);
        }
        else
            return x;
    }
}

for (var i = 0; i < 1000; ++i) {
    var g = makeClosure(42);
    var h = makeClosure(41);
    var value = g(h);
    if (value != 41)
        throw "Wrong result, got: " + value;
}
