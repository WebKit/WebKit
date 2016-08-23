function foo(p, a, b) {
    var result = 0;
    for (var i = 0; i < 1000; ++i) {
        if (p)
            result += a + b;
        else
            result += a - b;
    }
    return result;
}

noInline(foo);

var result = 0;

for (var i = 0; i < 1000; ++i) {
    result += foo(true, 1, 2);
    result += foo(false, 2000000000, 2000000000);
}

for (var i = 0; i < 10000; ++i)
    result += foo(false, 2000000000, 2000000000);

for (var i = 0; i < 10000; ++i)
    result += foo(true, 1, 2);

if (result != 33000000)
    throw "Error: bad result: " + result;
