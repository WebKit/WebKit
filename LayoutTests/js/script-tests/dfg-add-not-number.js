// This test won't get into the FTL currently, since the foo() function just exits a lot.
//@ noFTLRunLayoutTest

description(
"Tests that adding things that aren't numbers using ++ does not crash or generate bogus code."
);

function foo(a) {
    a++;
    return a;
}

dfgShouldBe(foo, "foo(\"foo\")", "NaN");
