//@ requireOptions("--useIteratorHelpers=1", "--useIteratorSequencing=1")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`FAIL: expected '${expected}' actual '${actual}'`);
}

function shouldBeArray(actual, expected) {
    shouldBe(actual.length, expected.length);
    for (let i = 0; i < expected.length; ++i)
        shouldBe(actual[i], expected[i]);
}

function shouldThrow(callback, errorConstructor) {
    try {
        callback();
    } catch (e) {
        shouldBe(e instanceof errorConstructor, true);
        return
    }
    throw new Error('FAIL: should have thrown');
}

shouldBeArray(Array.from(Iterator.concat([1, 2, 3, 4, 5, 6])), [1, 2, 3, 4, 5, 6]);
shouldBeArray(Array.from(Iterator.concat([1, 2], [3, 4], [5, 6])), [1, 2, 3, 4, 5, 6]);
shouldBeArray(Array.from(Iterator.concat([], [1], [], [2], [], [3], [], [4], [], [5], [], [6], [])), [1, 2, 3, 4, 5, 6]);

const customIterable = {
    [Symbol.iterator]() {
        return [1, 2, 3][Symbol.iterator]();
    },
};
shouldBeArray(Array.from(Iterator.concat(customIterable, ["abc"], customIterable)), [1, 2, 3, "abc", 1, 2, 3]);

const nonIterableArray = [];
nonIterableArray[Symbol.iterator] = undefined;

for (let invalid of [undefined, null, false, true, 42, "abc", {}, nonIterableArray]) {
    shouldThrow(() => {
        Iterator.concat(invalid);
    }, TypeError);
}
