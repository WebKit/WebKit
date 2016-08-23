//@ runNoFTL

function foo(a, b) {
    var [a, b] = [b, a];
    return a - b;
}

var result = 0;
for (var i = 0; i < 1000000; ++i)
    result += foo(42, i);

if (result != 499957500000)
    throw "Bad result: " + result;
