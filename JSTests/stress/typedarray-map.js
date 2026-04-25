load("./resources/typedarray-test-helper-functions.js", "caller relative");
description(
"This test checks the behavior of the TypedArray.prototype.map function"
);

shouldBe("Int32Array.prototype.map.length", "1");
shouldBe("Int32Array.prototype.map.name", "'map'");
shouldBeTrue("isSameFunctionForEachTypedArrayPrototype('map')");
shouldBeTrue("testPrototypeReceivesArray('map', [undefined, this, { }, [ ], true, ''])");
debug("");

debug("testPrototypeFunction has the following arg list (name, args, init, result [ , expectedArray ])");
debug("");

debug("1.0 Single Argument Testing");
function even(e, i) {
    return !(e & 1) || (this.change ? this.change.indexOf(i) >= 0 : false);
}
shouldBeTrue("testPrototypeFunction('map', '(even)', [12, 5, 8, 13, 44], [1, 0, 1, 0, 1], [12, 5, 8, 13, 44])");
shouldBeTrue("testPrototypeFunction('map', '(even)', [11, 54, 18, 13, 1], [0, 1, 1, 0, 0])");
debug("");

debug("2.0 Two Argument Testing");
var thisValue = { change: [1, 3] };
shouldBeTrue("testPrototypeFunction('map', '(even, thisValue)', [12, 23, 11, 1, 45], [1, 1, 0, 1, 0])");
debug("");

debug("3.0 Array Element Changing");
function evenAndChange(e, i, a) {
    a[a.length - 1 - i] = 5;
    return !(e & 1);
}
shouldBeTrue("testPrototypeFunction('map', '(evenAndChange)', [12, 15, 2, 13, 44], [1, 0, 1, 0, 0], [5, 5, 5, 5, 5])");
debug("");

debug("4.0 Exception Test");
function isBigEnoughAndException(element, index, array) {
    if(index==1) throw "exception from function";
    return (element >= 10);
}
shouldThrow("testPrototypeFunction('map', '(isBigEnoughAndException)', [12, 15, 10, 13, 44], false)");
debug("");

debug("5.0 Wrong Type for Callback Test");
shouldThrow("testPrototypeFunction('map', '(8)', [12, 15, 10, 13, 44], false)", "'TypeError: TypedArray.prototype.map callback must be a function'");
shouldThrow("testPrototypeFunction('map', '(\"wrong\")', [12, 15, 10, 13, 44], false)", "'TypeError: TypedArray.prototype.map callback must be a function'");
shouldThrow("testPrototypeFunction('map', '(new Object())', [12, 15, 10, 13, 44], false)", "'TypeError: TypedArray.prototype.map callback must be a function'");
shouldThrow("testPrototypeFunction('map', '(null)', [12, 15, 10, 13, 44], false)", "'TypeError: TypedArray.prototype.map callback must be a function'");
shouldThrow("testPrototypeFunction('map', '()', [12, 15, 10, 13, 44], false)", "'TypeError: TypedArray.prototype.map callback must be a function'");
debug("");

debug("6.0 Symbol.species Test");
subclasses = typedArrays.map(function(constructor) { return class extends constructor { } } );

function id(x) { return x; }

function testSpecies(array, constructor) {
    let newArray = array.map(id);
    return newArray instanceof constructor;
}
shouldBeTrue("forEachTypedArray(subclasses, testSpecies)");

Foo = class extends Int32Array { }
subclasses.forEach(function(constructor) { Object.defineProperty(constructor, Symbol.species, { value:Foo, writable:true }); });
function testSpeciesWithFoo(array, constructor) {
    let newArray = array.map(id);
    return newArray instanceof Foo;
}
shouldBeTrue("forEachTypedArray(subclasses, testSpeciesWithFoo)");
debug("");

debug("6.1 Symbol.species Test throws");
subclasses.forEach(function(constructor) { Object.defineProperty(constructor, Symbol.species, { value:1, writable:true }); });
shouldThrow("forEachTypedArray(subclasses, testSpecies)");

subclasses.forEach(function(constructor) { constructor[Symbol.species] = Array; });
shouldThrow("forEachTypedArray(subclasses, testSpecies)");
debug("");

debug("6.2 Symbol.species Test with Defaults");
subclasses.forEach(function(constructor) { constructor[Symbol.species] = null; });
function testSpeciesIsDefault(array, constructor) {
    let newArray = array.map(id);
    let defaultConstructor = typedArrays[subclasses.indexOf(constructor)];
    return newArray instanceof defaultConstructor;
}

shouldBeTrue("forEachTypedArray(subclasses, testSpeciesIsDefault)");

subclasses.forEach(function(constructor) { constructor[Symbol.species] = undefined; });
shouldBeTrue("forEachTypedArray(subclasses, testSpeciesIsDefault)");

subclasses.forEach(function(constructor) { constructor[Symbol.species] = () => new DataView(new ArrayBuffer()); });
function testSpeciesReturnsDataView(array, constructor) {
    try {
        array.map(id);
    } catch (e) {
        return e instanceof TypeError;
    }
    return false;
}
shouldBeTrue("forEachTypedArray(subclasses, testSpeciesReturnsDataView)");

subclasses = typedArrays.map(function(constructor) { return class extends constructor { } } );
function testSpeciesRemoveConstructor(array, constructor) {
    array.constructor = undefined;
    let newArray = array.map(id);
    let defaultConstructor = typedArrays[subclasses.indexOf(constructor)];
    return newArray instanceof defaultConstructor;
}

shouldBeTrue("forEachTypedArray(subclasses, testSpeciesRemoveConstructor)");
finishJSTest();
