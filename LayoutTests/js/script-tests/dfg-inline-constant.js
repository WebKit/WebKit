description(
"This tests that function inlining in the DFG JIT doesn't get confused by constants being reused between inliner and inlinee."
);

function foo(a, b) {
    if (b)
        return a + 4;
    return b + 5;
}

function bar(a, b) {
    return foo(a, b) + 5;
}

for (var i = 0; i < 1000; ++i)
    bar(i, i + 1);

shouldBe("bar(6, 0)", "10");
shouldBe("bar(6, 1)", "15");
shouldBe("bar(6, false)", "10");
shouldBe("bar(6, true)", "15");
