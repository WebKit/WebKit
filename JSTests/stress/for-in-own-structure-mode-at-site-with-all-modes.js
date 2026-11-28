function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}

function allModes(object) {
    let result = "";
    for (const key in object)
        result += key + object[key];
    return result;
}
noInline(allModes);

function ownThenOthers(object) {
    let result = "";
    for (const key in object)
        result += key + object[key];
    return result;
}
noInline(ownThenOthers);

Number.prototype.n = 8;

const plainObject = { a: 1, b: 2, c: 3 };
const otherPlainObject = { c: 3, b: 2, a: 1, d: 4 };
const objectWithDeletedProperty = { a: 1, x: 0, b: 2, c: 3 };
delete objectWithDeletedProperty.x;
const prototype = { p: 4, q: 5 };
const objectWithInheritedProperties = Object.create(prototype);
objectWithInheritedProperties.a = 1;
objectWithInheritedProperties.b = 2;
const objectWithIndexedProperties = { 0: 6, 1: 7, a: 1, b: 2 };
const array = [6, 7];
const proxy = new Proxy({ a: 1, b: 2 }, {});

for (let i = 0; i < testLoopCount; ++i) {
    shouldBe(allModes(plainObject), "a1b2c3");
    shouldBe(allModes(objectWithDeletedProperty), "a1b2c3");
    shouldBe(allModes(objectWithInheritedProperties), "a1b2p4q5");
    shouldBe(allModes(objectWithIndexedProperties), "0617a1b2");
    shouldBe(allModes(array), "0617");
    shouldBe(allModes(proxy), "a1b2");
    shouldBe(allModes("xy"), "0x1y");
    shouldBe(allModes(9), "n8");
    shouldBe(allModes(true), "");
    shouldBe(allModes(null), "");
    shouldBe(allModes(undefined), "");
    shouldBe(allModes(otherPlainObject), "c3b2a1d4");

    shouldBe(ownThenOthers(plainObject), "a1b2c3");
    if (i > testLoopCount / 2) {
        shouldBe(ownThenOthers(objectWithInheritedProperties), "a1b2p4q5");
        shouldBe(ownThenOthers(objectWithIndexedProperties), "0617a1b2");
        shouldBe(ownThenOthers(9), "n8");
        shouldBe(ownThenOthers(otherPlainObject), "c3b2a1d4");
    }
}
