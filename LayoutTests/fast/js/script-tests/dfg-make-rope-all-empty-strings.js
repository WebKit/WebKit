description(
"Check that the DFG can handle MakeRope or ValueAdd on all empty strings."
);

function foo(a, b) {
    return a + b;
}

function bar() {
    return foo("", "");
}

for (var i = 0; i < 100; ++i)
    shouldBe("bar()", "\"\"");

