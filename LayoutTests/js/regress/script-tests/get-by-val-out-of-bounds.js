function foo(a) {
    return a[1];
}

noInline(foo);

for (var i = 0; i < 100000; ++i) {
    var result = foo([42]);
    if (result !== void 0)
        throw "Error: bad value: " + result;
}
