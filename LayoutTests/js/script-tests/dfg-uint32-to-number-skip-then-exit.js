description(
"This tests that a skipped conversion of uint32 to number does not confuse OSR exit into thinking that the conversion is dead."
);

function foo(a, b, o) {
    var x = a >>> b;
    return o.f + (x | 0);
}

for (var i = 0; i < 200; ++i) {
    var o;
    var expected;
    if (i < 150) {
        o = {f:42};
        expected = 42 + ((i / 2) | 0);
    } else {
        o = {f:43, g:44};
        expected = 43 + ((i / 2) | 0);
    }
    shouldBe("foo(i, 1, o)", "" + expected);
}

