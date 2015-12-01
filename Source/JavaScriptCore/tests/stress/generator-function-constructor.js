function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}
var global = (new Function("return this"))();
shouldBe(typeof global.GeneratorFunction, 'undefined');
var generatorFunctionConstructor = (function *() { }).constructor;
shouldBe(generatorFunctionConstructor.__proto__, Function);
shouldBe(generatorFunctionConstructor.prototype.constructor, generatorFunctionConstructor);
