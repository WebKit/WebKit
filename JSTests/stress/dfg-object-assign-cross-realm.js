function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

const other = createGlobalObject();

Object.assign = other.Object.assign;

function test(x) {
    return Object.getPrototypeOf(Object.assign(x, { a: 1 }));
}
noInline(test);

for (let i = 0; i < testLoopCount; ++i)
    shouldBe(test(5), other.Number.prototype);
