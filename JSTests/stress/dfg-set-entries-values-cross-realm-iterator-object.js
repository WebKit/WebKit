function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

new Set().values();

const other = createGlobalObject();
const otherIteratorProto = Object.getPrototypeOf(new other.Set().values());

Set.prototype.entries = other.Set.prototype.entries;
Set.prototype.values = other.Set.prototype.values;

function testEntries(set) {
    return set.entries();
}
noInline(testEntries);

function testValues(set) {
    return set.values();
}
noInline(testValues);

const set = new Set([1]);
for (let i = 0; i < testLoopCount; ++i) {
    shouldBe(Object.getPrototypeOf(testEntries(set)), otherIteratorProto);
    shouldBe(Object.getPrototypeOf(testValues(set)), otherIteratorProto);
}
