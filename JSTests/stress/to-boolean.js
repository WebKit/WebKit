function shouldBe(value, expected)
{
    let result = !!value;
    if (result !== expected)
        throw new Error("!!" + String(value) + " = " + result + ", expected " + expected);
}

// Fast path: Boolean values.
shouldBe(false, false);
shouldBe(true, true);

// Falsy slow-path cases.
shouldBe(undefined, false);
shouldBe(null, false);
shouldBe(0, false);
shouldBe(-0, false);
shouldBe(NaN, false);
shouldBe("", false);

// Truthy slow-path cases.
shouldBe(1, true);
shouldBe(-1, true);
shouldBe(42, true);
shouldBe(Infinity, true);
shouldBe(-Infinity, true);
shouldBe("x", true);
shouldBe({}, true);
shouldBe([], true);
shouldBe(function () {}, true);

// with JIT
for (let i = 0; i < 1e6; ++i) {
    shouldBe(false, false);
    shouldBe(true, true);
    shouldBe(0, false);
    shouldBe(1, true);
    shouldBe("", false);
    shouldBe("x", true);
    shouldBe(null, false);
    shouldBe({}, true);
}