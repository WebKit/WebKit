function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)}, expected: ${String(expected)}`);
}

function shouldBeArray(actual, expected) {
    shouldBe(actual.length, expected.length);
    for (let i = 0; i < expected.length; ++i)
        shouldBe(actual[i], expected[i]);
}

function pushInt32(array, value) { return array.push(value); }
noInline(pushInt32);

function pushDouble(array, value) { return array.push(value); }
noInline(pushDouble);

function pushContiguous(array, value) { return array.push(value); }
noInline(pushContiguous);

function pushTwo(array, first, second) { return array.push(first, second); }
noInline(pushTwo);

function pushTwoDoubles(array, first, second) { return array.push(first, second); }
noInline(pushTwoDoubles);

function pushMixed(array, value) { return array.push(value); }
noInline(pushMixed);

function pushNothing(array) { return array.push(); }
noInline(pushNothing);

function literalThenPush(value) {
    const array = ["a", "b", "c"];
    array.push(value);
    return array;
}
noInline(literalThenPush);

const object = { a: 1, b: 2, c: 3 };

for (let i = 0; i < testLoopCount; ++i) {
    {
        const array = Object.keys(object);
        shouldBe(pushContiguous(array, "d"), 4);
        shouldBeArray(array, ["a", "b", "c", "d"]);
        shouldBeArray(Object.keys(object), ["a", "b", "c"]);
    }
    {
        const array = [1, 2, 3];
        shouldBe(pushInt32(array, 4), 4);
        shouldBeArray(array, [1, 2, 3, 4]);
        shouldBe(4 in array, false);
    }
    {
        const array = [1.5, 2.5, 3.5];
        shouldBe(pushDouble(array, 4.5), 4);
        shouldBeArray(array, [1.5, 2.5, 3.5, 4.5]);
        shouldBe(4 in array, false);
    }
    {
        const array = "x,y".split(",");
        shouldBe(pushTwo(array, "z", i), 4);
        shouldBeArray(array, ["x", "y", "z", i]);
        shouldBeArray("x,y".split(","), ["x", "y"]);
    }
    {
        const array = [0.5];
        shouldBe(pushTwoDoubles(array, 1.5, 2.5), 3);
        shouldBeArray(array, [0.5, 1.5, 2.5]);
    }
    {
        const array = i % 3 ? (i % 3 === 1 ? [1, 2] : [1.5, 2.5]) : ["a", "b"];
        const value = i % 2 ? "string" : i % 4 ? 1.5 : 1;
        const expected = [...array, value];
        shouldBe(pushMixed(array, value), 3);
        shouldBeArray(array, expected);
    }
    {
        const array = [1, 2, 3];
        shouldBe(pushNothing(array), 3);
        shouldBeArray(array, [1, 2, 3]);
    }
    shouldBeArray(literalThenPush(i), ["a", "b", "c", i]);
}

for (const length of [0, 1, 2, 3, 4, 5, 7, 8, 9, 16, 17, 64, 100, 1000, 1004, 2000]) {
    const keys = {};
    for (let i = 0; i < length; ++i)
        keys["k" + i] = i;
    const expected = Array.from({ length }, (_, i) => "k" + i);
    const array = Object.keys(keys);
    for (let i = 0; i < length + 3; ++i) {
        shouldBe(pushContiguous(array, i), length + i + 1);
        shouldBe(array.length in array, false);
    }
    shouldBeArray(array, expected.concat(Array.from({ length: length + 3 }, (_, i) => i)));
    shouldBeArray(Object.keys(keys), expected);
}

const kept = [];
for (let i = 0; i < 200; ++i) {
    const array = Object.keys(object);
    pushContiguous(array, { value: i });
    kept.push(array);
}
gc();
for (let i = 0; i < 200; ++i) {
    shouldBe(kept[i].length, 4);
    shouldBe(kept[i][0], "a");
    shouldBe(kept[i][3].value, i);
}
