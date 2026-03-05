// Test that RegExp flag getters correctly handle type checks when non-RegExpObject is passed.

function getGlobal(x) { return x.global; }
noInline(getGlobal);

var re = /foo/g;

// Warm up with RegExpObject to get DFG/FTL compilation
for (var i = 0; i < 1e5; ++i) {
    var result = getGlobal(re);
    if (result !== true)
        throw new Error("Expected true but got " + result);
}

// Now test with a non-RegExpObject that has a 'global' property
var obj = { global: 42 };
var result = getGlobal(obj);
if (result !== 42)
    throw new Error("Expected 42 but got " + result);
