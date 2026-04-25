function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function* generator() { }

var GeneratorPrototype = Object.getPrototypeOf(generator).prototype;
generator.prototype = null;

shouldBe(Object.getPrototypeOf(generator()), GeneratorPrototype);
