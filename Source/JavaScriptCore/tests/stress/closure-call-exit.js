function foo(o, i) {
    return o[i]();
}

noInline(foo);

for (var i = 0; i < 10000; ++i) {
    var result = foo([function() { return 42; }], 0);
    if (result != 42)
        throw "Error: bad result: " + result;
}

var result = foo([function() { return 43; }], 0);
if (result != 43)
    throw "Error: bad result at end: " + result;
