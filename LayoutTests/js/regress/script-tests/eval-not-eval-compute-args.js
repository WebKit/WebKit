function foo(eval, a, b, c) {
    var x = 42;
    var result = eval(a, b, c);
    for (var i = 0; i < 100000; ++i)
        result++;
    return result;
}

noInline(foo);

for (var i = 0; i < 200; ++i) {
    var result = foo(function(a, b, c) { return a + b + c; }, 42, i, 0);
    if (result != 42 + i + 100000)
        throw "Error: bad result: " + result;
}

