function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

const other = createGlobalObject();

Object.getOwnPropertyNames = other.Object.getOwnPropertyNames;

function test(object) {
    return Object.getOwnPropertyNames(object);
}
noInline(test);

const object = { a: 1, b: 2 };
for (let i = 0; i < testLoopCount; ++i)
    shouldBe(Object.getPrototypeOf(test(object)), other.Array.prototype);
