function foo(a, v) {
    a[0] = v + 2000000000;
}

noInline(foo);

for (var i = 0; i < 100000; ++i) {
    var a = new Int32Array(1);
    foo(a, 2000000000);
    if (a[0] != -294967296)
        throw "Error: bad value: " + a[0];
}
