description(
"Tests that the DFG knows that a function that appears like Math.sqrt could potentially use value in arbitrary ways, and not just in a context that converts values to numbers."
);

function foo(f, array, i) {
    return f(array[i]);
}

function bar(value) {
    return value;
}

for (var i = 0; i < 200; ++i) {
    var f, array, expected;
    if (i == 190) {
        f = "bar";
        array = "[, 1.5]";
        expected = "void 0";
    } else {
        f = "Math.sqrt";
        array = "[1.5]";
        expected = "Math.sqrt(1.5)";
    }
    shouldBe("foo(" + f + ", " + array + ", 0)", expected);
}

