description(
"Tests that adding things that aren't numbers using ++ does not crash or generate bogus code."
);

function foo(a) {
    a++;
    return a;
}

for (var i = 0; i < 100; ++i)
    shouldBe("foo(\"foo\" + i)", "NaN");

