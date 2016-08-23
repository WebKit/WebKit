function foo(o, p) {
    var result = 0;
    for (var i = 0; i < 100; ++i) {
        result += o.f;
        p.g = 42;
    }
    return result;
}

noInline(foo);

function test(o) {
    if (foo(o, {}) != 100)
        throw new Error("Error: bad result: " + result);
}

for (var i = 0; i < 100; ++i) {
    test({f:1, g:2});
    test({f:1, h:2});
    test({f:1, i:2});
    test({f:1, j:2});
    test({f:1, k:2});
    test({f:1, l:2});
}

for (var i = 0; i < 10000; ++i)
    test({f:1, g:2});
