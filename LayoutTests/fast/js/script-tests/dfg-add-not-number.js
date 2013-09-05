description(
"Tests that adding things that aren't numbers using ++ does not crash or generate bogus code."
);

function foo(a) {
    a++;
    return a;
}

dfgShouldBe(foo, "foo(\"foo\")", "NaN");
