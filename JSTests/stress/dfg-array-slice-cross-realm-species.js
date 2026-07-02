function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

const other = createGlobalObject();

Array.prototype.slice = other.Array.prototype.slice;

function test(array) {
    return array.slice(0);
}
noInline(test);

const array = [1, 2, 3];
for (let i = 0; i < testLoopCount; ++i)
    shouldBe(Object.getPrototypeOf(test(array)), other.Array.prototype);
