description(
"Tests correctness of function calls when the function is overwritten."
);

function foo(a, b) {
    return a + b;
}

function bar(a, b) {
    return foo(a, b);
}

for (var i = 0; i < 200; ++i) {
    if (i == 150)
        foo = function(a, b) { return a - b; }
    var expected;
    if (i < 150)
        expected = i + i + 1;
    else
        expected = -1;
    shouldBe("bar(i, i + 1)", "" + expected);
}

