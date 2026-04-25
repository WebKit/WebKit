function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

let constructorCallCount = 0;
function Constructor() {
    constructorCallCount++;
    return {};
}

const array = [1, 2, 3];
array.constructor = { [Symbol.species]: Constructor };

const result = array.concat();

shouldBe(constructorCallCount, 1);
shouldBe(Object.getPrototypeOf(result) === Array.prototype, false);
shouldBe(result[0], 1);
shouldBe(result[1], 2);
shouldBe(result[2], 3);
