function foo(a, b) {
    return a + "x" + b;
}

noInline(foo);

for (var i = 0; i < 10000; ++i) {
    var b;
    var expected;
    if (i & 1) {
        b = 42;
        expected = "ax42";
    } else {
        b = "b";
        expected = "axb";
    }
    var result = foo("a", b);
    if (result != expected)
        throw "Error: bad result: " + result;
}

var longStr = "l";
for (var i = 0; i < 30; ++i)
    longStr = longStr + longStr;

var result = null;
var didThrow = false;
try {
    result = foo(longStr, longStr);
} catch (e) {
    didThrow = true;
}

if (!didThrow)
    throw "Error: did not throw";
if (result !== null)
    throw "Error: did set result: " + result;
