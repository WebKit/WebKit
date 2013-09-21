description(
"Tests effect of mutating Arguments object's structure."
);
function foo() {
	arguments.a=true;
	return arguments.a;
}

dfgShouldBe(foo, "foo()", "true");
