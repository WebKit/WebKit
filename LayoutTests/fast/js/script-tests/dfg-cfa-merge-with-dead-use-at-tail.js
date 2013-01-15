description(
"Tests that a dead use of a variable at the tail of a basic block doesn't confuse the CFA into believing that the variable being used is dead as well."
);

function foo(p, q, v) {
    var x, y;
    if (p)
        x = 0;
    else {
        if (q)
            x = v;
        else
            x = 0;
        y = x;
    }
    if (x)
        return 42;
    return 0;
}

for (var i = 0; i < 200; ++i)
    shouldBe("foo(false, true, 5)", "42");
