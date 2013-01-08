description(
"Tests that the DFG knows that a function that appears like Math.abs could potentially use value in arbitrary ways, and not just in a context that converts values to numbers."
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
        f = "Math.abs";
        array = "[1.5]";
        expected = "Math.abs(1.5)";
    }
    shouldBe("foo(" + f + ", " + array + ", 0)", expected);
}

