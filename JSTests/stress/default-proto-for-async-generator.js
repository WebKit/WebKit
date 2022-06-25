function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

async function* asyncGenerator() { }

var AsyncGeneratorPrototype = Object.getPrototypeOf(asyncGenerator).prototype;
asyncGenerator.prototype = null;

shouldBe(Object.getPrototypeOf(asyncGenerator()), AsyncGeneratorPrototype);
