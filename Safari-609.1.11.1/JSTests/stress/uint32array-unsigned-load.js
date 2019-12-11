function foo(a) {
    return a[0] + 1;
}

noInline(foo);

var a = new Uint32Array(1);
a[0] = -1;

for (var i = 0; i < 10000; ++i) {
    var result = foo(a);
    if (result != 4294967296)
        throw "Error: bad result: " + result;
}

