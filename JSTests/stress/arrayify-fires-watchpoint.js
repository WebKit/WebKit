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

function makeObjectArray(value) {
    var result = {};
    result[0] = value;
    return result;
}

var p = {f:42};
p[0] = 5;
for (var i = 0; i < 100000; ++i) {
    test(makeObjectArray(4), p, 88);
    test(makeObjectArray(4.5), p, 88.5);
}

test(p, p, 89);
