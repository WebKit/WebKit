load("./resources/typedarray-test-helper-functions.js", "caller relative");
description(
"This test checks the behavior of the TypedArray.prototype.filter function"
);

shouldBe("Int32Array.prototype.filter.length", "1");
shouldBe("Int32Array.prototype.filter.name", "'filter'");
shouldBeTrue("isSameFunctionForEachTypedArrayPrototype('filter')");
shouldBeTrue("testPrototypeReceivesArray('filter', [undefined, this, { }, [ ], true, ''])");
debug("");

debug("testPrototypeFunction has the following arg list (name, args, init, result [ , expectedArray ])");
debug("");

debug("1.0 Single Argument Testing");
function keepEven(e, i) {
    return !(e & 1) || (this.keep ? this.keep.indexOf(i) >= 0 : false);
}
shouldBeTrue("testPrototypeFunction('filter', '(keepEven)', [12, 5, 8, 13, 44], [12, 8, 44])");
shouldBeTrue("testPrototypeFunction('filter', '(keepEven)', [11, 54, 18, 13, 1], [54, 18])");
debug("");

debug("2.0 Two Argument Testing");
var thisValue = { keep: [1, 3] };
shouldBeTrue("testPrototypeFunction('filter', '(keepEven, thisValue)', [12, 23, 11, 1, 45], [12, 23, 1])");
debug("");

debug("3.0 Array Element Changing");
function keepEvenAndChange(e, i, a) {
    a[a.length - 1 - i] = 5;
    return !(e & 1);
}
shouldBeTrue("testPrototypeFunction('filter', '(keepEvenAndChange)', [12, 15, 2, 13, 44], [12, 2], [5, 5, 5, 5, 5])");
debug("");

debug("4.0 Exception Test");
function isBigEnoughAndException(element, index, array) {
    if(index==1) throw "exception from function";
    return (element >= 10);
}
shouldThrow("testPrototypeFunction('filter', '(isBigEnoughAndException)', [12, 15, 10, 13, 44], false)");
debug("");

debug("5.0 Wrong Type for Callback Test");
shouldThrow("testPrototypeFunction('filter', '(8)', [12, 15, 10, 13, 44], false)", "'TypeError: TypedArray.prototype.filter callback must be a function'");
shouldThrow("testPrototypeFunction('filter', '(\"wrong\")', [12, 15, 10, 13, 44], false)", "'TypeError: TypedArray.prototype.filter callback must be a function'");
shouldThrow("testPrototypeFunction('filter', '(new Object())', [12, 15, 10, 13, 44], false)", "'TypeError: TypedArray.prototype.filter callback must be a function'");
shouldThrow("testPrototypeFunction('filter', '(null)', [12, 15, 10, 13, 44], false)", "'TypeError: TypedArray.prototype.filter callback must be a function'");
shouldThrow("testPrototypeFunction('filter', '()', [12, 15, 10, 13, 44], false)", "'TypeError: TypedArray.prototype.filter callback must be a function'");
debug("");

debug("6.0 Symbol.species Test");
subclasses = typedArrays.map(function(constructor) { return class extends constructor { } } );

function accept() { return true; }

function testSpecies(array, constructor) {
    let newArray = array.filter(accept);
    return newArray instanceof constructor;
}

shouldBeTrue("forEachTypedArray(subclasses, testSpecies)");

Foo = class extends Int32Array { }
subclasses.forEach(function(constructor) { Object.defineProperty(constructor, Symbol.species, { value:Foo, writable:true }); });
function testSpeciesWithFoo(array, constructor) {
    let newArray = array.filter(accept);
    return newArray instanceof Foo;
}
shouldBeTrue("forEachTypedArray(subclasses, testSpeciesWithFoo)");
debug("");

debug("6.2 Symbol.species Test throws");
subclasses.forEach(function(constructor) { constructor[Symbol.species] = 1; });
shouldThrow("forEachTypedArray(subclasses, testSpecies)");

subclasses.forEach(function(constructor) { constructor[Symbol.species] = Array; });
shouldThrow("forEachTypedArray(subclasses, testSpecies)");
debug("");

debug("6.2 Symbol.species Test with Defaults");
subclasses.forEach(function(constructor) { constructor[Symbol.species] = null; });
function testSpeciesIsDefault(array, constructor) {
    let newArray = array.filter(accept);
    let defaultConstructor = typedArrays[subclasses.indexOf(constructor)];
    return newArray instanceof defaultConstructor;
}

shouldBeTrue("forEachTypedArray(subclasses, testSpeciesIsDefault)");

subclasses.forEach(function(constructor) { constructor[Symbol.species] = undefined; });
shouldBeTrue("forEachTypedArray(subclasses, testSpeciesIsDefault)");

subclasses.forEach(function(constructor) { constructor[Symbol.species] = () => new DataView(new ArrayBuffer()); });
function testSpeciesReturnsDataView(array, constructor) {
    try {
        array.filter(accept);
    } catch (e) {
        return e instanceof TypeError;
    }
    return false;
}
shouldBeTrue("forEachTypedArray(subclasses, testSpeciesReturnsDataView)");

subclasses = typedArrays.map(function(constructor) { return class extends constructor { } } );
function testSpeciesRemoveConstructor(array, constructor) {
    array.constructor = undefined;
    let newArray = array.filter(accept);
    let defaultConstructor = typedArrays[subclasses.indexOf(constructor)];
    return newArray instanceof defaultConstructor;
}

shouldBeTrue("forEachTypedArray(subclasses, testSpeciesRemoveConstructor)");

finishJSTest();
