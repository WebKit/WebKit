function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var GeneratorFunction = (function *() { }).constructor;

class DerivedGeneratorFunction extends GeneratorFunction {
    constructor()
    {
        super("yield 42");
    }

    hello()
    {
        return 50;
    }
}

let DerivedGenerator = new DerivedGeneratorFunction();
shouldBe(DerivedGenerator.__proto__, DerivedGeneratorFunction.prototype);
shouldBe(DerivedGenerator.hello(), 50);
let gen = DerivedGenerator();
shouldBe(gen.next().value, 42);
