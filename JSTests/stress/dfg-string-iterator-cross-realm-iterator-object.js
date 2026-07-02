function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

"x"[Symbol.iterator]();

const other = createGlobalObject();
const otherStringIterator = other.String.prototype[Symbol.iterator];
const otherIteratorProto = Object.getPrototypeOf(otherStringIterator.call("x"));

String.prototype[Symbol.iterator] = otherStringIterator;

function test(string) {
    return string[Symbol.iterator]();
}
noInline(test);

for (let i = 0; i < testLoopCount; ++i)
    shouldBe(Object.getPrototypeOf(test("abc")), otherIteratorProto);
