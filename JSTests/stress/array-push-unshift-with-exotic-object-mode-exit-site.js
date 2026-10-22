function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}

const spreadable = { length: 1, 0: 4, [Symbol.isConcatSpreadable]: true };

function testConcat(call) {
    shouldBe(call([1, 2, 3], "concat", spreadable).join(), "1,2,3,4");
}

function testMutator(call, name, expected) {
    const array = [1, 2, 3];
    shouldBe(call(array, name, 4), 4);
    shouldBe(array.join(), expected);
}

function concatThenPush(array, name, value) {
    return array[name](value);
}
noInline(concatThenPush);

function concatThenUnshift(array, name, value) {
    return array[name](value);
}
noInline(concatThenUnshift);

function concatOrPush(array, name, value) {
    return array[name](value);
}
noInline(concatOrPush);

function concatOrUnshift(array, name, value) {
    return array[name](value);
}
noInline(concatOrUnshift);

for (let i = 0; i < testLoopCount; ++i) {
    testConcat(concatThenPush);
    testConcat(concatThenUnshift);
}

for (let i = 0; i < testLoopCount; ++i) {
    testMutator(concatThenPush, "push", "1,2,3,4");
    testMutator(concatThenUnshift, "unshift", "4,1,2,3");
}

for (let i = 0; i < testLoopCount; ++i) {
    testConcat(concatOrPush);
    testMutator(concatOrPush, "push", "1,2,3,4");
    testConcat(concatOrUnshift);
    testMutator(concatOrUnshift, "unshift", "4,1,2,3");
}
