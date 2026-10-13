function shouldBe(actual, expected, what) {
    if (actual !== expected)
        throw new Error(what + ": bad value " + actual + ", expected " + expected);
}

function notOfAdd(x, y) { return (!(x + y)) | 0; }
noInline(notOfAdd);

function notOfMul(x, y) { return (!(x * y)) | 0; }
noInline(notOfMul);

function notOfDiv(x, y) { return (!(x / y)) | 0; }
noInline(notOfDiv);

for (let i = 1; i < testLoopCount; ++i) {
    shouldBe(notOfAdd(i, 1), 0, "warm notOfAdd");
    shouldBe(notOfMul(i, 1), 0, "warm notOfMul");
    shouldBe(notOfDiv(i, 1), 0, "warm notOfDiv");
}

shouldBe(notOfAdd(-2147483648, -2147483648), 0, "!(-2^31 + -2^31) | 0");
shouldBe(notOfMul(65536, 65536), 0, "!(2^16 * 2^16) | 0");
shouldBe(notOfDiv(1, 2), 0, "!(1 / 2) | 0");
shouldBe(notOfAdd(0, 0), 1, "!(0 + 0) | 0");
shouldBe(notOfDiv(0, 5), 1, "!(0 / 5) | 0");
