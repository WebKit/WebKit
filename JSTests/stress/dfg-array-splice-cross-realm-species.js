function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

const other = createGlobalObject();

Array.prototype.splice = other.Array.prototype.splice;

function test(array) {
    return array.splice(0, 0);
}
noInline(test);

const array = [1, 2, 3];
for (let i = 0; i < testLoopCount; ++i)
    shouldBe(Object.getPrototypeOf(test(array)), other.Array.prototype);
