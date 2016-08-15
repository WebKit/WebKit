function foo(a, s) {
    return a[s] + 1;
}

var shouldThrow = false;
function bar() {
    if (shouldThrow)
        throw "hello";
    return 42;
}

var a = {};
a.__defineGetter__("bar", bar);

noInline(foo);
noInline(bar);

for (var i = 0; i < 100000; ++i) {
    var result = foo(a, "bar");
    if (result != 43)
        throw "Error: bad result: " + result;
}

var didThrow;
try {
    shouldThrow = true;
    foo(a, "bar");
} catch (e) {
    didThrow = e;
}

if (didThrow != "hello")
    throw "Error: didn't throw or threw wrong exception: " + didThrow;
