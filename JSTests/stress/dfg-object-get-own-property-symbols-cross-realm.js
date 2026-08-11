function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

const other = createGlobalObject();

Object.getOwnPropertySymbols = other.Object.getOwnPropertySymbols;

function test(object) {
    return Object.getOwnPropertySymbols(object);
}
noInline(test);

const object = { [Symbol("a")]: 1, [Symbol("b")]: 2 };
for (let i = 0; i < testLoopCount; ++i)
    shouldBe(Object.getPrototypeOf(test(object)), other.Array.prototype);
