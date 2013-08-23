description(
"Tests that the (x/-1)|0 case in the DFG is handled correctly even when there is some interesting register allocation going on."
);

function foo(c, d, a, b) {
    return (c + d) + ((a / b) | 0);
}

for (var i = 0; i < 100; ++i)
    shouldBe("foo(0, 0, " + i + ", -1)", (i ? "-" : "") + i);

