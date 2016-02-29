function foo(s) {
    return /foo/.test(s);
}

noInline(foo);

for (var i = 0; i < 100000; ++i) {
    if (!foo("foo"))
        throw "Error: bad result for foo";
    if (foo("bar"))
        throw "Error: bad result for bar";
}
