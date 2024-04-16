function shouldBe(actual, expected) {
    if (actual !== expected) {
        throw new Error('bad value: ' + actual);
    }
}

const runs = 1e6;

function testZeroIndex(str) {
    let count = 0;
    for (let i = 0; i < runs; i++) {
        const maybeH = str.at(0);
        if (maybeH === str[0]) {
            count++;
        }
    }
    return count;
}

function testPositiveIndex(str) {
    let count = 0;
    for (let i = 0; i < runs; i++) {
        const index = 3;
        const value = str.at(index);
        if (value === str[index]) {
            count++;
        }
    }
    return count;
}

function testOutOfBounds(str) {
    let count = 0;
    for (let i = 0; i < runs; i++) {
        const index = 20;
        const value = str.at(index);
        if (value === undefined) {
            count++;
        }
    }
    return count;
}

noInline(testOutOfBounds);

function testNegativeIndex(str) {
    let count = 0;
    for (let i = 0; i < runs; i++) {
        const index = -3;
        const value = str.at(index);
        if (value === str[str.length + index]) {
            count++;
        }
    }
    return count;
}

noInline(testNegativeIndex);

const string8 = "hello, world";
const string16 = "こんにちは、世界";

shouldBe(testZeroIndex(string16), runs);
shouldBe(testPositiveIndex(string16), runs);
shouldBe(testOutOfBounds(string16), runs);
shouldBe(testNegativeIndex(string16), runs);

shouldBe(testZeroIndex(string8), runs);
shouldBe(testPositiveIndex(string8), runs);
shouldBe(testOutOfBounds(string8), runs);
shouldBe(testNegativeIndex(string8), runs);
