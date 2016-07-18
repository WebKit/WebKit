function foo(eval, string) {
    var x = 42;
    var result = eval(string);
    for (var i = 0; i < 100000; ++i)
        result++;
    return result;
}

noInline(foo);

for (var i = 0; i < 200; ++i) {
    var result = foo(function() { return 42 + i; }, "x + " + i);
    if (result != 42 + i + 100000)
        throw "Error: bad result: " + result;
}

