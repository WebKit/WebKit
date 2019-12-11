function foo(p) {
    var result = 42;
    var o = {};
    if (p)
        result = isInt32(o);
    return result;
}

noInline(foo);

var result = foo(true);
if (result !== false)
    throw "Error: bad result at end: " + result;

for (var i = 0; i < 10000; ++i) {
    var result = foo(false);
    if (result !== 42)
        throw "Error: bad result: " + result;
}

var result = foo(true);
if (result !== false)
    throw "Error: bad result at end: " + result;

