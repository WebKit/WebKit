description(
"Tests that a polymorphic DFG PutById that allocates, and then reallocates, property storage works."
);

function foo(o) {
    o.a = 1;
    o.b = 2;
    o.c = 3;
    o.d = 4;
    o.e = 5;
    o.f = 6;
    o.g = 7;
    o.h = 8;
    o.i = 9;
    o.j = 10;
    o.k = 11;
    o.l = 12;
    o.m = 13;
    o.n = 14;
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
    shouldBe("o.h", "8");
    shouldBe("o.i", "9");
    shouldBe("o.j", "10");
    shouldBe("o.k", "11");
    shouldBe("o.l", "12");
    shouldBe("o.m", "13");
    shouldBe("o.n", "14");
    if (!(i % 2))
        shouldBe("o.foo", "42");
    else
        shouldBe("o.foo", "void 0");
}


