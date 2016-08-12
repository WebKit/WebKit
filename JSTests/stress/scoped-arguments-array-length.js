function foo(a) {
    var result = 0;
    if (!a)
        return function() { return a };
    for (var i = 0; i < arguments.length; ++i)
        result += arguments[i];
    return result;
}

noInline(foo);

for (var i = 0; i < 10000; ++i) {
    var result = foo(42, i);
    if (result != 42 + i)
        throw "Error: bad result: " + result;
}

