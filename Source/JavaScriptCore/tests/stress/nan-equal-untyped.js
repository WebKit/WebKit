function foo(x) {
    return x == x;
}

noInline(foo);

function test(value) {
    var result = foo(value);
    if (result !== true)
        throw "Error: bad result for " + value + ": " + result;
}

for (var i = 0; i < 10000; ++i) {
    test(true);
    test(4);
    test("hello");
    test(new Object());
    test(1.5);
}

var result = foo(0/0);
if (result !== false)
    throw "Error: bad result at end: " + result;

