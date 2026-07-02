function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

new Map().values().next();

const other = createGlobalObject();
const otherIterator = other.String.prototype.matchAll.call("aa", new other.RegExp("a", "g"));
const otherNext = otherIterator.next;

Object.getPrototypeOf("aa".matchAll(/a/g)).next = otherNext;

function test(string) {
    return string.matchAll(/a/g).next();
}
noInline(test);

for (let i = 0; i < testLoopCount; ++i) {
    const result = test("aa");
    shouldBe(Object.getPrototypeOf(result), other.Object.prototype);
    shouldBe(result.done, false);
}
