function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

const other = createGlobalObject();

Array.prototype.concat = other.Array.prototype.concat;

function test(array, array2) {
    return array.concat(array2);
}
noInline(test);

const array = [1, 2, 3];
const array2 = [4, 5];
for (let i = 0; i < testLoopCount; ++i)
    shouldBe(Object.getPrototypeOf(test(array, array2)), other.Array.prototype);
