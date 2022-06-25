function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function above(a, b) {
    return (a >>> 0) > (b >>> 0);
}
noInline(above);

function aboveOrEqual(a, b) {
    return (a >>> 0) >= (b >>> 0);
}
noInline(aboveOrEqual);

function below(a, b) {
    return (a >>> 0) < (b >>> 0);
}
noInline(below);

function belowOrEqual(a, b) {
    return (a >>> 0) <= (b >>> 0);
}
noInline(belowOrEqual);

(function aboveTest() {
    for (let i = 0; i < 1e5; ++i) {
        shouldBe(above(0, 20), false);
        shouldBe(above(0, 0), false);
        shouldBe(above(0, -0), false);
        shouldBe(above(-1, 0), true);
        shouldBe(above(-1, -1), false);
        shouldBe(above(-1, 1), true);
        shouldBe(above(1, -1), false);
        shouldBe(above(1, 0xffffffff), false);
        shouldBe(above(0xffffffff, 0xffffffff), false);
        shouldBe(above(-1, 0xffffffff), false);
        shouldBe(above(-1, 0xfffffffff), false);
    }
}());

(function aboveOrEqualTest() {
    for (let i = 0; i < 1e5; ++i) {
        shouldBe(aboveOrEqual(0, 20), false);
        shouldBe(aboveOrEqual(0, 0), true);
        shouldBe(aboveOrEqual(0, -0), true);
        shouldBe(aboveOrEqual(-1, 0), true);
        shouldBe(aboveOrEqual(-1, -1), true);
        shouldBe(aboveOrEqual(-1, 1), true);
        shouldBe(aboveOrEqual(1, -1), false);
        shouldBe(aboveOrEqual(1, 0xffffffff), false);
        shouldBe(aboveOrEqual(0xffffffff, 0xffffffff), true);
        shouldBe(aboveOrEqual(-1, 0xffffffff), true);
        shouldBe(aboveOrEqual(-1, 0xfffffffff), true);
    }
}());

(function belowTest() {
    for (let i = 0; i < 1e5; ++i) {
        shouldBe(below(0, 20), true);
        shouldBe(below(0, 0), false);
        shouldBe(below(0, -0), false);
        shouldBe(below(-1, 0), false);
        shouldBe(below(-1, -1), false);
        shouldBe(below(-1, 1), false);
        shouldBe(below(1, -1), true);
        shouldBe(below(1, 0xffffffff), true);
        shouldBe(below(0xffffffff, 0xffffffff), false);
        shouldBe(below(-1, 0xffffffff), false);
        shouldBe(below(-1, 0xfffffffff), false);
    }
}());

(function belowOrEqualTest() {
    for (let i = 0; i < 1e5; ++i) {
        shouldBe(belowOrEqual(0, 20), true);
        shouldBe(belowOrEqual(0, 0), true);
        shouldBe(belowOrEqual(0, -0), true);
        shouldBe(belowOrEqual(-1, 0), false);
        shouldBe(belowOrEqual(-1, -1), true);
        shouldBe(belowOrEqual(-1, 1), false);
        shouldBe(belowOrEqual(1, -1), true);
        shouldBe(belowOrEqual(1, 0xffffffff), true);
        shouldBe(belowOrEqual(0xffffffff, 0xffffffff), true);
        shouldBe(belowOrEqual(-1, 0xffffffff), true);
        shouldBe(belowOrEqual(-1, 0xfffffffff), true);
    }
}());
