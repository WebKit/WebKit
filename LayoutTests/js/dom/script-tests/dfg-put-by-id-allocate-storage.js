description(
"Tests that a DFG PutById that allocates property storage works."
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
}


