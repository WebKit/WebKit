function foo(a, b, string)
{
    OSRExit();
    return eval(string);
}

noInline(foo);

for (var i = 0; i < 100000; ++i) {
    var result = foo(1, 2, "a + b + 1");
    if (result != 1 + 2 + 1)
        throw "Error: bad result in loop: " + result;
}

