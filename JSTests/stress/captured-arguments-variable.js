function foo(a) {
    return arguments[1] + (function() { return a * 101; })();
}

noInline(foo);

for (var i = 0; i < 10000; ++i) {
    var result = foo(42, 97);
    if (result != 4339)
        throw "Error: bad result: " + result;
}

Object.prototype[1] = 111;

var result = foo(42);
if (result != 4353)
    throw "Error: bad result at end: " + result;
