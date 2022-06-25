description(
"Tests that a polymorphic DFG PutById that allocates property storage works."
);

function foo(o) {
    o.a = 1;
    o.b = 2;
    o.c = 3;
    o.d = 4;
    o.e = 5;
    o.f = 6;
    o.g = 7;
}

for (var i = 0; i < 150; ++i) {
    var o;
    if (i % 2)
        o = {};
    else
        o = {foo: 42};
    foo(o);
    shouldBe("o.a", "1");
    shouldBe("o.b", "2");
    shouldBe("o.c", "3");
    shouldBe("o.d", "4");
    shouldBe("o.e", "5");
    shouldBe("o.f", "6");
    shouldBe("o.g", "7");
    if (!(i % 2))
        shouldBe("o.foo", "42");
    else
        shouldBe("o.foo", "void 0");
}


