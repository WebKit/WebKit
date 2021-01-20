function foo(a, b) {
    return a + b;
}

function verify(a, b) {
    if (a !== b)
        throw "Error: the two arguments objects aren't identical.";
    if (a[0] !== 42)
        throw "Error: the first argument isn't 42 (a).";
    if (b[0] !== 42)
        throw "Error: the first argument isn't 42 (b).";
}

noInline(verify);

var global = false;
function bar(x) {
    var a = arguments;
    if (global) {
        x = 42;
        verify(arguments, a);
    }
    return foo.apply(null, a);
}

function baz(a, b) {
    return bar(a, b);
}

noInline(baz);

for (var i = 0; i < 10000; ++i) {
    var result = baz(1, 2);
    if (result != 1 + 2)
        throw "Error: bad result: " + result;
}

global = true;
var result = baz(1, 2);
if (result != 42 + 2)
    throw "Error: bad result at end: " + result;
