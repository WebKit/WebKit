description(
"Tests that Math.cos() on a constant works in the DFG."
);

function foo() { return Math.cos(0); }

dfgShouldBe(foo, "foo()", "1");
