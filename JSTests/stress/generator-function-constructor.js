function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function shouldThrow(func, errorMessage) {
    var errorThrown = false;
    var error = null;
    try {
        func();
    } catch (e) {
        errorThrown = true;
        error = e;
    }
    if (!errorThrown)
        throw new Error('not thrown');
    if (String(error) !== errorMessage)
        throw new Error(`bad error: ${String(error)}`);
}

var global = (new Function("return this"))();
shouldBe(typeof global.GeneratorFunction, 'undefined');
var generatorFunctionConstructor = (function *() { }).constructor;
shouldBe(generatorFunctionConstructor.__proto__, Function);
shouldBe(generatorFunctionConstructor.prototype.constructor, generatorFunctionConstructor);
shouldBe(generatorFunctionConstructor() instanceof generatorFunctionConstructor, true);
shouldBe(generatorFunctionConstructor("a") instanceof generatorFunctionConstructor, true);
shouldBe(generatorFunctionConstructor("a", "b") instanceof generatorFunctionConstructor, true);

// Generator functions created by the GeneratorFunction constructor are not themselves constructors.
shouldThrow(() => new (generatorFunctionConstructor()), "TypeError: function is not a constructor (evaluating 'new (generatorFunctionConstructor())')");