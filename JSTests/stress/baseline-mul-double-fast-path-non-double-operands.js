function mul(a, b) { return a * b; }
noInline(mul);

function shouldBe(actual, expected) {
    if (!Object.is(actual, expected))
        throw new Error("bad value: " + actual + " expected " + expected);
}

for (var i = 0; i < testLoopCount / 10; ++i)
    shouldBe(mul(1.5, 2.5), 3.75);

var valueOfCalls = 0;
var object = { valueOf() { ++valueOfCalls; return 3; } };
var iterations = 0;
for (var i = 0; i < testLoopCount / 10; ++i) {
    ++iterations;
    shouldBe(mul(1.5, 2), 3);
    shouldBe(mul(2, 1.5), 3);
    shouldBe(mul(3, 4), 12);
    shouldBe(mul(NaN, 2.5), NaN);
    shouldBe(mul(1.5, NaN), NaN);
    shouldBe(mul(-0.5, 0), -0);
    shouldBe(mul(1.5, "2"), 3);
    shouldBe(mul(true, 2.5), 2.5);
    shouldBe(mul(null, 2.5), 0);
    shouldBe(mul(undefined, 2.5), NaN);
    shouldBe(mul(1.5, object), 4.5);
    shouldBe(mul(2n, 3n), 6n);
}
shouldBe(valueOfCalls, iterations);
