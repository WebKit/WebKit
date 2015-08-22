function foo(a, b) {
    return a + b;
}

noInline(foo);

var bStr = "b";
for (var i = 0; i < 30; ++i)
    bStr = bStr + bStr;

var effects = 0;
var b = {toString: function() { effects++; return bStr; }};

for (var i = 0; i < 10000; ++i) {
    effects = 0;
    var result = foo("a", b);
    if (result.length != "a".length + bStr.length)
        throw "Error: bad result in loop: " + result;
    if (effects != 1)
        throw "Error: bad number of effects: " + effects;
}

// Create a large string.
var a = "a";
for (var i = 0; i < 30; ++i)
    a = a + a;

effects = 0;
var result = null;
var didThrow = false;
try {
    result = foo(a, b);
} catch (e) {
    didThrow = true;
}

if (!didThrow)
    throw "Error: did not throw.";
if (result !== null)
    throw "Error: did set result to something: " + result;
if (effects != 1)
    throw "Error: bad number of effects at end: " + effects;
