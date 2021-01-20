function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function *generatorFunction() {
}
let generator = generatorFunction();

shouldBe(generator instanceof generatorFunction, true);
shouldBe(typeof generator.__proto__, 'object');
shouldBe(generator.__proto__, generatorFunction.prototype);

let GeneratorPrototype = generator.__proto__.__proto__;

let GeneratorFunctionPrototype = generatorFunction.__proto__;
let GeneratorFunction = generatorFunction.__proto__.constructor;
shouldBe(GeneratorFunction.prototype, GeneratorFunctionPrototype);
shouldBe(generatorFunction instanceof GeneratorFunction, true);
shouldBe(GeneratorFunction.__proto__, Function);
shouldBe(GeneratorFunctionPrototype.__proto__, Function.prototype);

shouldBe(GeneratorFunctionPrototype.prototype, GeneratorPrototype);
shouldBe(GeneratorPrototype.constructor, GeneratorFunctionPrototype);

let arrayIterator = [][Symbol.iterator]();
let ArrayIteratorPrototype = arrayIterator.__proto__;
let IteratorPrototype = ArrayIteratorPrototype.__proto__;

shouldBe(IteratorPrototype, GeneratorPrototype.__proto__);
