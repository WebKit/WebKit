function foo(a, b, string)
{
    var x = a + b;
    return eval(string);
}

noInline(foo);

for (var i = 0; i < 100000; ++i) {
    var result = foo(1, 2, "x + 1");
    if (result != 1 + 2 + 1)
        throw "Error: bad result in loop: " + result;
}

var result = foo(2000000000, 2000000000, "x - 1");
if (result != 2000000000 + 2000000000 - 1)
    throw "Error: bad result at end: " + result;
