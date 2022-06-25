description(
"Tests that a variable predicted to be either int or double but proven to be an int does confuse the Branch logic."
);

function foo(a, b, c) {
    a = b;
    if (c) {
        if (a)
            return 1;
        else
            return 2;
    } else if (b)
        a = 0.5;
    return a;
}

shouldBe("foo(0, 1, 0)", "0.5");

for (var i = 0; i < 200; ++i)
    shouldBe("foo(i, i + 1, i + 2)", "1");

