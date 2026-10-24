function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + actual + " expected: " + expected);
}

function untilLess(values, bound) {
    let index = 0;
    let value;
    do {
        value = values[index++];
    } while (!(value < bound));
    return index;
}

function untilLessEq(values, bound) {
    let index = 0;
    let value;
    do {
        value = values[index++];
    } while (!(value <= bound));
    return index;
}

function untilGreater(values, bound) {
    let index = 0;
    let value;
    do {
        value = values[index++];
    } while (!(value > bound));
    return index;
}

function untilGreaterEq(values, bound) {
    let index = 0;
    let value;
    do {
        value = values[index++];
    } while (!(value >= bound));
    return index;
}

function untilEq(values, bound) {
    let index = 0;
    let value;
    do {
        value = values[index++];
    } while (!(value == bound));
    return index;
}

function untilStrictEq(values, bound) {
    let index = 0;
    let value;
    do {
        value = values[index++];
    } while (!(value === bound));
    return index;
}

function untilNotEq(values, bound) {
    let index = 0;
    let value;
    do {
        value = values[index++];
    } while (!(value != bound));
    return index;
}

function untilNotStrictEq(values, bound) {
    let index = 0;
    let value;
    do {
        value = values[index++];
    } while (!(value !== bound));
    return index;
}

function untilBelow(values, bound) {
    let index = 0;
    let value;
    do {
        value = values[index++];
    } while (!((value >>> 0) < (bound >>> 0)));
    return index;
}

function untilBelowEq(values, bound) {
    let index = 0;
    let value;
    do {
        value = values[index++];
    } while (!((value >>> 0) <= (bound >>> 0)));
    return index;
}

for (let i = 0; i < testLoopCount; ++i) {
    shouldBe(untilLess([5, NaN, 3, 2], 3), 4);
    shouldBe(untilLess([5.5, 2.5], 3.5), 2);
    shouldBe(untilLessEq([5, NaN, 4, 3], 3), 4);
    shouldBe(untilGreater([1, NaN, 3, 4], 3), 4);
    shouldBe(untilGreaterEq([1, NaN, 2, 3], 3), 4);
    shouldBe(untilEq([1, "2", NaN, "3"], 3), 4);
    shouldBe(untilStrictEq(["3", NaN, 2, 3], 3), 4);
    shouldBe(untilNotEq([3, "3", 3, 4], 3), 4);
    shouldBe(untilNotStrictEq([3, 3, 3, "3"], 3), 4);
    shouldBe(untilBelow([-1, 5, 3, 2], 3), 4);
    shouldBe(untilBelowEq([-1, 5, 4, 3], 3), 4);
}
