function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}

function ownAndGeneric(object) {
    let result = "";
    for (const key in object)
        result += key + object[key];
    return result;
}
noInline(ownAndGeneric);

function ownAndIndexed(object) {
    let result = "";
    for (const key in object)
        result += key + object[key];
    return result;
}
noInline(ownAndIndexed);

function ownAndThrowing(object) {
    let result = "";
    try {
        for (const key in object)
            result += key + object[key];
    } catch (error) {
        result += error;
    }
    return result;
}
noInline(ownAndThrowing);

const plainObject = { a: 1, b: 2, c: 3 };
const otherPlainObject = { c: 3, b: 2, a: 1, d: 4 };
const objectWithDeletedProperty = { a: 1, x: 0, b: 2, c: 3 };
delete objectWithDeletedProperty.x;
const prototype = { p: 4, q: 5 };
const objectWithInheritedProperties = Object.create(prototype);
objectWithInheritedProperties.a = 1;
objectWithInheritedProperties.b = 2;
const objectWithOneOwnProperty = Object.create(prototype);
objectWithOneOwnProperty.a = 1;
const objectWithIndexedProperties = { 0: 6, 1: 7, a: 1, b: 2 };
const array = [6, 7];
const objectWithThrowingPrototype = Object.create(new Proxy({ p: 4 }, {
    getOwnPropertyDescriptor() {
        throw "!";
    }
}));
objectWithThrowingPrototype.a = 1;
objectWithThrowingPrototype.b = 2;

for (let i = 0; i < testLoopCount; ++i) {
    shouldBe(ownAndGeneric(plainObject), "a1b2c3");
    shouldBe(ownAndGeneric(objectWithDeletedProperty), "a1b2c3");
    shouldBe(ownAndGeneric(otherPlainObject), "c3b2a1d4");
    shouldBe(ownAndGeneric(objectWithInheritedProperties), "a1b2p4q5");
    shouldBe(ownAndGeneric(objectWithOneOwnProperty), "a1p4q5");
    shouldBe(ownAndGeneric({}), "");

    shouldBe(ownAndIndexed(plainObject), "a1b2c3");
    shouldBe(ownAndIndexed(array), "0617");
    shouldBe(ownAndIndexed(objectWithIndexedProperties), "0617a1b2");
    shouldBe(ownAndIndexed(otherPlainObject), "c3b2a1d4");

    shouldBe(ownAndThrowing(plainObject), "a1b2c3");
    shouldBe(ownAndThrowing(objectWithThrowingPrototype), "a1b2!");
}
