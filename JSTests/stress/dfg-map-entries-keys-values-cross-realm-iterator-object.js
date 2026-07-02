function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

new Map().values();

const other = createGlobalObject();
const otherIteratorProto = Object.getPrototypeOf(new other.Map().values());

Map.prototype.entries = other.Map.prototype.entries;
Map.prototype.keys = other.Map.prototype.keys;
Map.prototype.values = other.Map.prototype.values;

function testEntries(map) {
    return map.entries();
}
noInline(testEntries);

function testKeys(map) {
    return map.keys();
}
noInline(testKeys);

function testValues(map) {
    return map.values();
}
noInline(testValues);

const map = new Map([[1, 2]]);
for (let i = 0; i < testLoopCount; ++i) {
    shouldBe(Object.getPrototypeOf(testEntries(map)), otherIteratorProto);
    shouldBe(Object.getPrototypeOf(testKeys(map)), otherIteratorProto);
    shouldBe(Object.getPrototypeOf(testValues(map)), otherIteratorProto);
}
