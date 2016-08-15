function foo(a, b) {
    var x = b.f;
    x += a[0];
    return x + b.f;
}

noInline(foo);

function test(a, b, c) {
    var result = foo(a, b);
    if (result != c)
        throw new Error("bad result: expected " + c + " but got: " + result);
}

var p = {f:42};
p[0] = 5;
for (var i = 0; i < 100000; ++i) {
    test([4], p, 88);
    test([4.5], p, 88.5);
}

test(p, p, 89);
