description(
"Tests that adding things that aren't numbers using ++ does not crash or generate bogus code."
);

function foo(a) {
    a++;
    return a;
}

silentTestPass = true;
noInline(foo);

for (var i = 0; i < 100; i = dfgIncrement({f:foo, i:i + 1, n:50}))
    shouldBe("foo(\"foo\" + i)", "NaN");

