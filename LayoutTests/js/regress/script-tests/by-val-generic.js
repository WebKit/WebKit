function foo(a, b, c) {
    a[b] = c;
    return a[b];
}

noInline(foo);

for (var i = 0; i < 100000; ++i) {
    var result = foo({}, "foo", "bar");
    if (result !== "bar")
        throw "Error: bad result: " + result;
}
