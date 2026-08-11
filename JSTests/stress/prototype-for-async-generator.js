function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

async function* asyncGenerator() { }

var AsyncGeneratorPrototype = Object.getPrototypeOf(asyncGenerator).prototype;

shouldBe(Object.getPrototypeOf(asyncGenerator.prototype), AsyncGeneratorPrototype);

class A {
    async *asyncGenerator()
    {
    }
}

var a = new A;
shouldBe(Object.getPrototypeOf(a.asyncGenerator.prototype), AsyncGeneratorPrototype);
