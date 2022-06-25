description(
"Tests that Math.sin() on a constant works in the DFG."
);

function foo() { return Math.sin(0); }

dfgShouldBe(foo, "foo()", "0");
