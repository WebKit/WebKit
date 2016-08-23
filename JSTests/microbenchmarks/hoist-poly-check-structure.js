function foo(o) {
    var result = 0;
    for (var i = 0; i < 100; ++i)
        result += o.f;
    return result;
}

noInline(foo);

function test(o) {
    if (foo(o) != 100)
        throw new Error("Error: bad result: " + result);
}

for (var i = 0; i < 100; ++i) {
    test({f:1});
    test({f:1, g:2});
    test({f:1, g:2, h:3});
    test({f:1, g:2, h:3, i:4});
    test({f:1, g:2, h:3, i:4, j:5});
    test({f:1, g:2, h:3, i:4, j:5, k:6});
}

for (var i = 0; i < 10000; ++i)
    test({f:1});
