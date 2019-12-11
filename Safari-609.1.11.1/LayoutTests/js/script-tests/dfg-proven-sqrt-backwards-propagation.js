description(
"Tests that the DFG knows that a Math.sqrt could potentially use value in arbitrary ways, and not just in a context that converts values to numbers."
);

function foo(array, i) {
    var x = array[i];
    return Math.sqrt(x);
}

function bar(value) {
    return value;
}

for (var i = 0; i < 200; ++i) {
    if (i == 190)
        Math.sqrt = bar;
    var array, expected;
    if (i >= 190) {
        array = [, 1.5];
        expected = "void 0";
    } else {
        array = [1.5];
        expected = "Math.sqrt(1.5)";
    }
    shouldBe("foo(array, 0)", expected);
}

