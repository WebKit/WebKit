description(
"Tests that the DFG handles x % 1 correctly."
);

function foo(x) { return x % 1; }

dfgShouldBe(foo, "foo(-5.5)", "-0.5");
dfgShouldBe(foo, "1 / foo(-1)", "-Infinity");
