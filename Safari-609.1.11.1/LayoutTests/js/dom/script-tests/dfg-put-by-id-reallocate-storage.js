description(
"Tests that a DFG PutById that allocates, and then reallocates, property storage works."
);

function foo() {
    var o = {};
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
    return o;
}

for (var i = 0; i < 150; ++i) {
    var o = foo();
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
}


