load("./resources/typedarray-test-helper-functions.js");
description(
"This test checks the behavior of the TypedArray.prototype.subarray function"
);

shouldBe("Int32Array.prototype.subarray.length", "2");
shouldBe("Int32Array.prototype.subarray.name", "'subarray'");
shouldBeTrue("isSameFunctionForEachTypedArrayPrototype('subarray')");
shouldBeTrue("testPrototypeReceivesArray('subarray', [undefined, this, { }, [ ], true, ''])");
debug("");

debug("1.0 Symbol.species Test");
subclasses = typedArrays.map(function(constructor) { return class extends constructor { } } );

function testSpecies(array, constructor) {
    let newArray = array.subarray(0, 0);
    return newArray instanceof constructor;
}
shouldBeTrue("forEachTypedArray(subclasses, testSpecies)");

Foo = class extends Int32Array { }
subclasses.forEach(function(constructor) { Object.defineProperty(constructor, Symbol.species, { value:Foo, writable:true }); });
function testSpeciesWithFoo(array, constructor) {
    let newArray = array.subarray(0, 0);
    return newArray instanceof Foo;
}
shouldBeTrue("forEachTypedArray(subclasses, testSpeciesWithFoo)");
debug("");

debug("4.1 Symbol.species Test throws");
subclasses.forEach(function(constructor) { Object.defineProperty(constructor, Symbol.species, { value:1, writable:true }); });
shouldThrow("forEachTypedArray(subclasses, testSpecies)");

subclasses.forEach(function(constructor) { constructor[Symbol.species] = Array; });
shouldThrow("forEachTypedArray(subclasses, testSpecies)");
debug("");

debug("4.2 Symbol.species Test with Defaults");
subclasses.forEach(function(constructor) { constructor[Symbol.species] = null; });
function testSpeciesIsDefault(array, constructor) {
    let newArray = array.subarray(0, 0);
    let defaultConstructor = typedArrays[subclasses.indexOf(constructor)];
    return newArray instanceof defaultConstructor;
}

shouldBeTrue("forEachTypedArray(subclasses, testSpeciesIsDefault)");

subclasses.forEach(function(constructor) { constructor[Symbol.species] = undefined; });
shouldBeTrue("forEachTypedArray(subclasses, testSpeciesIsDefault)");

subclasses = typedArrays.map(function(constructor) { return class extends constructor { } } );
function testSpeciesRemoveConstructor(array, constructor) {
    array.constructor = undefined;
    let newArray = array.subarray(0, 0);
    let defaultConstructor = typedArrays[subclasses.indexOf(constructor)];
    return newArray instanceof defaultConstructor;
}

shouldBeTrue("forEachTypedArray(subclasses, testSpeciesRemoveConstructor)");

finishJSTest();
