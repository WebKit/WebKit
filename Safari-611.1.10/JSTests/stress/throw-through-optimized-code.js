function foo(f) {
    return f(1) + 1;
}

var shouldThrow = false;
function bar(x) {
    if (shouldThrow)
        throw "hello";
    return 42 - x;
}

noInline(foo);
noInline(bar);

for (var i = 0; i < 100000; ++i) {
    var result = foo(bar);
    if (result != 42)
        throw "Error: bad result: " + result;
}

var didThrow;
try {
    shouldThrow = true;
    foo(bar);
} catch (e) {
    didThrow = e;
}

if (didThrow != "hello")
    throw "Error: didn't throw or threw wrong exception: " + didThrow;
