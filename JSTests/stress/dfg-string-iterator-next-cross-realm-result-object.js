function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

"x"[Symbol.iterator]().next();

const other = createGlobalObject();
const otherNext = other.String.prototype[Symbol.iterator].call("x").next;

Object.getPrototypeOf("x"[Symbol.iterator]()).next = otherNext;

function test(string) {
    return string[Symbol.iterator]().next();
}
noInline(test);

for (let i = 0; i < testLoopCount; ++i) {
    const result = test("ab");
    shouldBe(Object.getPrototypeOf(result), other.Object.prototype);
    shouldBe(result.done, false);
}
