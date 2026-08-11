load("./resources/typedarray-test-helper-functions.js", "caller relative");
description(
"This test checks the behavior of the TypedArray.prototype.findLast function"
);

shouldBe("Int32Array.prototype.findLast.length", "1");
shouldBe("Int32Array.prototype.findLast.name", "'findLast'");
shouldBeTrue("isSameFunctionForEachTypedArrayPrototype('findLast')");
shouldBeTrue("testPrototypeReceivesArray('findLast', [undefined, this, { }, [ ], true, ''])");
debug("");

debug("testPrototypeFunction has the following arg list (name, args, init, result [ , expectedArray ])");
debug("");

debug("1.0 Single Argument Testing");
function keepEven(e, i) {
    return !(e & 1) || (this.keep ? this.keep === i : false);
}
shouldBeTrue("testPrototypeFunction('findLast', '(keepEven)', [12, 5, 8, 13, 44], 44)");
shouldBeTrue("testPrototypeFunction('findLast', '(keepEven)', [11, 13, 17, 13, 22], 22)");
shouldBeTrue("testPrototypeFunction('findLast', '(keepEven)', [22, 13, 17, 13, 11], 22)");
shouldBeTrue("testPrototypeFunction('findLast', '(keepEven)', [11, 13, 17, 13, 11], undefined)");
debug("");

debug("2.0 Two Argument Testing");
var thisValue = { keep: 3 };
shouldBeTrue("testPrototypeFunction('findLast', '(keepEven, thisValue)', [11, 23, 11, 1, 44], 44)");
shouldBeTrue("testPrototypeFunction('findLast', '(keepEven, thisValue)', [11, 23, 11, 1, 43], 1)");
debug("");

debug("3.0 Array Element Changing");
function keepEvenAndChange(e, i, a) {
    a[a.length - 1 - i] = 5;
    return !(e & 1);
}
shouldBeTrue("testPrototypeFunction('findLast', '(keepEvenAndChange)', [11, 15, 3, 12, 44], 44, [5, 15, 3, 12, 44])");
shouldBeTrue("testPrototypeFunction('findLast', '(keepEvenAndChange)', [44, 12, 3, 15, 11], undefined, [5, 5, 5, 5, 5])");
debug("");

debug("4.0 Exception Test");
function isBigEnoughAndException(element, index, array) {
    if(index==3) throw "exception from function";
    return (element == 44);
}
shouldBeTrue("testPrototypeFunction('findLast', '(isBigEnoughAndException)', [12, 15, 10, 13, 44], 44)");
shouldThrow("testPrototypeFunction('findLast', '(isBigEnoughAndException)', [9, 15, 10, 13, 43], false)");
debug("");

debug("5.0 Wrong Type for Callback Test");
shouldThrow("testPrototypeFunction('findLast', '(8)', [12, 15, 10, 13, 44], false)", "'TypeError: TypedArray.prototype.findLast callback must be a function'");
shouldThrow("testPrototypeFunction('findLast', '(\"wrong\")', [12, 15, 10, 13, 44], false)", "'TypeError: TypedArray.prototype.findLast callback must be a function'");
shouldThrow("testPrototypeFunction('findLast', '(new Object())', [12, 15, 10, 13, 44], false)", "'TypeError: TypedArray.prototype.findLast callback must be a function'");
shouldThrow("testPrototypeFunction('findLast', '(null)', [12, 15, 10, 13, 44], false)", "'TypeError: TypedArray.prototype.findLast callback must be a function'");
shouldThrow("testPrototypeFunction('findLast', '()', [12, 15, 10, 13, 44], false)", "'TypeError: TypedArray.prototype.findLast callback must be a function'");
debug("");
finishJSTest();
