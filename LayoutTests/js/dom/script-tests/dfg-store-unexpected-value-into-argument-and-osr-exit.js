description(
"Tests what happens when you store an unexpected value into an argument (where the original argument value was the expected one) and then OSR exit."
);

function foo(x, o, p) {
    x = o.f;
    if (p) {
        var result = o.g + x;
        x = true; // Force x to not have a hoisted integer speculation.
        return result;
    } else
        return o.g - x;
}

for (var i = 0; i < 200; ++i) {
    var expected;
    var p, f, g;
    if (i < 150) {
        f = 42;
        g = 43;
        if (i%2) {
            p = true;
            expected = 85;
        } else {
            p = false;
            expected = 1;
        }
    } else {
        f = 42.5;
        g = 43;
        if (i%2) {
            p = true;
            expected = 85.5;
        } else {
            p = false;
            expected = 0.5;
        }
    }
    shouldBe("foo(3, {f:f, g:g}, p)", "" + expected);
}

