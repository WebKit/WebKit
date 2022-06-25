function foo(a)
{
    (function() { return a; })();
    return [arguments[0], arguments];
}

noInline(foo);

for (var i = 0; i < 10000; ++i) {
    var result = foo(42);
    if (result[0] != 42)
        throw new Error("result[0] is not 42: " + result[0]);
    if (result[1][0] != 42)
        throw new Error("result[1][0] is not 42: " + result[1][0]);
}

