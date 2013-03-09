description(
"Tests that if we have a tower of large numerical constants being added to each other, the DFG knows that a sufficiently large tower may produce a large enough value that overflow check elimination must be careful."
);

function foo(a, b) {
    return (a + b + 281474976710655 + 281474976710655 + 281474976710655 + 281474976710655 +
            281474976710655 + 281474976710655 + 281474976710655 + 281474976710655 +
            281474976710655 + 281474976710655 + 281474976710655 + 281474976710655 +
            281474976710655 + 281474976710655 + 281474976710655 + 281474976710655 +
            281474976710655 + 281474976710655 + 281474976710655 + 281474976710655 +
            281474976710655 + 281474976710655 + 281474976710655 + 281474976710655 +
            281474976710655 + 281474976710655 + 281474976710655 + 281474976710655 +
            281474976710655 + 281474976710655 + 281474976710655 + 281474976710655 + 30) | 0;
}

function bar(a, b, o) {
    eval(""); // Prevent this function from being opt compiled.
    return foo(a, b, o);
}

var warmup = 200;

for (var i = 0; i < warmup + 1; ++i) {
    var a, b, c;
    var expected;
    if (i < warmup) {
        a = 1;
        b = 2;
        expected = 0;
    } else {
        a = 2147483645;
        b = 2147483644;
        expected = -10;
    }
    shouldBe("bar(" + a + ", " + b + ")", "" + expected);
}

