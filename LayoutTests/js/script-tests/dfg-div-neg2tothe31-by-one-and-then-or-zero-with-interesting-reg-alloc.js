description(
"Tests that the (-2147483648/-1)|0 case in the DFG is handled correctly even when there is some interesting register allocation going on."
);

function foo(c, d, a, b) {
    return (c + d) + ((a / b) | 0);
}

dfgShouldBe(foo, "foo(0, 0, -2147483647-1, -1)", "-2147483647-1");

