function foo(x) {
    return function(y) { return x + y; }
}

function bar(a, b) {
    return foo(a)(b);
}

noInline(foo);
noInline(bar);

for (var i = 0; i < 100000; ++i) {
    var result = bar(i, i + 1);
    if (result != i * 2 + 1)
        throw "Error: bad result for " + i + ": " + result;
}
