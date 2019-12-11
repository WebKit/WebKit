function foo(a, b) {
    return a + "x" + b;
}

noInline(foo);

for (var i = 0; i < 10000; ++i) {
    var result = foo({toString: function() { return "a"; }}, 42);
    if (result != "ax42")
        throw "Error: bad result: " + result;
}

var result = null;
var didThrow = false;
try {
    result = foo({toString: function() { throw "error"; }}, 42);
} catch (e) {
    didThrow = true;
}

if (!didThrow)
    throw "Error: did not throw";
if (result !== null)
    throw "Error: did set result: " + result;
