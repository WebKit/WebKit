var flag = true;
flag = false;

function foo(a, b, string)
{
    var x = a + b;
    if (flag)
        return eval(string);
    return 42;
}

noInline(foo);

for (var i = 0; i < 100000; ++i) {
    var result = foo(1, 2, "x + 1");
    if (result != 42)
        throw "Error: bad result in loop: " + result;
}

flag = true;
var result = foo(1, 2, "x - 1");
if (result != 1 + 2 - 1)
    throw "Error: bad result at end: " + result;
