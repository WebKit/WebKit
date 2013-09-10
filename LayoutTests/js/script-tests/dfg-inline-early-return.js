description(
"This tests that an early return in the first basic block does not crash the DFG's bytecode parser whilst inlining."
);

function foo(a) {
    {
        return a;
    }
}

function bar(a) {
    return foo(a);
}

for (var i = 0; i < 100; ++i)
    bar(i);

shouldBe("foo(42)", "42");
shouldBe("bar(42)", "42");

var successfullyParsed = true;
