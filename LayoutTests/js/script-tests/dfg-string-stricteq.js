description(
"Tests that the DFG does the right thing on strict equality for known strings."
);

function foo(a, b) {
    a = a.f;
    b = b.f;
    var c = a.length + b.length;
    return [c, a === b];
}

function bar(a, b) {
    a = a.f;
    b = b.f;
    var c = a.length + b.length;
    if (a === b)
        return c + 1;
    else
        return c - 1;
}

for (var i = 0; i < 1000; ++i) {
    var a = "blah" + i;
    var b = "blah" + (1000 - i);
    var areEqual = i == 500;
    shouldBe("foo({f:\"" + a + "\"}, {f:\"" + b + "\"})", "[" + (a.length + b.length) + ", " + areEqual + "]");
    shouldBe("bar({f:\"" + a + "\"}, {f:\"" + b + "\"})", "" + (areEqual ? a.length + b.length + 1 : a.length + b.length - 1));
}

