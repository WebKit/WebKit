function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${actual}, expected ${expected}`);
}

function shouldThrowRangeError(func) {
    let threw = false;
    try {
        func();
    } catch (error) {
        threw = true;
        if (!(error instanceof RangeError))
            throw new Error(`bad error: ${error}`);
    }
    if (!threw)
        throw new Error("did not throw");
}

const outOfRangeArguments = [Infinity, -Infinity, 1e100, -1e100, 2147483648, -2147483648, 4294967296, 1e21, 101, -1];
for (const argument of outOfRangeArguments) {
    shouldThrowRangeError(() => (1.5).toExponential(argument));
    shouldThrowRangeError(() => (1.5).toFixed(argument));
    shouldThrowRangeError(() => (1.5).toPrecision(argument));
}
shouldThrowRangeError(() => (1.5).toPrecision(0));

shouldBe((1.5).toExponential(100), "1.5000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000e+0");
shouldBe((1.5).toFixed(100), "1.5000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000");
shouldBe((1.5).toPrecision(100), "1.500000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000");
shouldBe((1.5).toPrecision(1), "2");

// NaN/Infinity receivers are handled before the range check for toExponential/toPrecision, after it for toFixed.
shouldBe((NaN).toExponential(Infinity), "NaN");
shouldBe((Infinity).toExponential(1e100), "Infinity");
shouldBe((-Infinity).toExponential(-Infinity), "-Infinity");
shouldBe((NaN).toPrecision(Infinity), "NaN");
shouldBe((Infinity).toPrecision(1e100), "Infinity");
shouldThrowRangeError(() => (NaN).toFixed(Infinity));
shouldThrowRangeError(() => (Infinity).toFixed(1e100));
