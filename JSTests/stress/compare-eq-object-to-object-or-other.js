function foo(a, b) {
    return a == b;
}

noInline(foo);

function test(a, b, expected) {
    var result = foo(a, b);
    if (result != expected)
        throw new Error("Unexpected result: " + result);
}

for (var i = 0; i < 100000; ++i) {
    var o = {f:42};
    var p = {g:43};
    test(o, o, true);
    test(o, p, false);
    test(p, o, false);
    test(p, p, true);
    test(o, null, false);
    test(p, null, false);
    test(o, void 0, false);
    test(p, void 0, false);
}
