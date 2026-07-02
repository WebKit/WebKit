function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

new Map().values().next();
new Set().values().next();

const other = createGlobalObject();
const otherMapNext = new other.Map().values().next;
const otherSetNext = new other.Set().values().next;

Object.getPrototypeOf(new Map().values()).next = otherMapNext;
Object.getPrototypeOf(new Set().values()).next = otherSetNext;

function testMap(map) {
    return map.values().next();
}
noInline(testMap);

function testSet(set) {
    return set.values().next();
}
noInline(testSet);

const map = new Map([[1, 2]]);
const set = new Set([1]);
for (let i = 0; i < testLoopCount; ++i) {
    const mapResult = testMap(map);
    shouldBe(Object.getPrototypeOf(mapResult), other.Object.prototype);
    shouldBe(mapResult.done, false);
    const setResult = testSet(set);
    shouldBe(Object.getPrototypeOf(setResult), other.Object.prototype);
    shouldBe(setResult.done, false);
}
