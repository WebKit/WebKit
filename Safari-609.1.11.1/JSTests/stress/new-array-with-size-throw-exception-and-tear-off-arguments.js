function foo() {
    var a = arguments;
    return new Array(a[0]);
}

function bar(x) {
    return foo(x);
}

noInline(bar);

for (var i = 0; i < 10000; ++i) {
    var result = bar(42);
    if (result.length != 42)
        throw "Error: bad result length: " + result;
}

var didThrow = false;
try {
    bar(-1);
} catch (e) {
    didThrow = e;
}

if (("" + didThrow).indexOf("RangeError") != 0)
    throw "Error: did not throw right exception: " + didThrow;
