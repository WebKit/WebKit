description(
"Tests that the DFG handles x % 1, where x is an integer, correctly."
);

function foo(x) { return x % 1; }

dfgShouldBe(foo, "foo(1)", "0");
