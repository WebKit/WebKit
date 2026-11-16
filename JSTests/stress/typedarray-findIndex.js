load("./resources/typedarray-test-helper-functions.js", "caller relative");
description(
"This test checks the behavior of the TypedArray.prototype.findIndex function"
);

shouldBe("Int32Array.prototype.findIndex.length", "1");
shouldBe("Int32Array.prototype.findIndex.name", "'findIndex'");
shouldBeTrue("isSameFunctionForEachTypedArrayPrototype('findIndex')");
shouldBeTrue("testPrototypeReceivesArray('findIndex', [undefined, this, { }, [ ], true, ''])");
debug("");

debug("testPrototypeFunction has the following arg list (name, args, init, result [ , expectedArray ])");
debug("");

debug("1.0 Single Argument Testing");
function keepEven(e, i) {
    return !(e & 1) || (this.keep ? this.keep === i : false);
}
shouldBeTrue("testPrototypeFunction('findIndex', '(keepEven)', [12, 5, 8, 13, 44], 0)");
shouldBeTrue("testPrototypeFunction('findIndex', '(keepEven)', [11, 13, 17, 13, 22], 4)");
shouldBeTrue("testPrototypeFunction('findIndex', '(keepEven)', [11, 13, 17, 13, 11], -1)");
debug("");

debug("2.0 Two Argument Testing");
var thisValue = { keep: 3 };
shouldBeTrue("testPrototypeFunction('findIndex', '(keepEven, thisValue)', [11, 23, 11, 1, 44], 3)");
debug("");

debug("3.0 Array Element Changing");
function keepEvenAndChange(e, i, a) {
    a[a.length - 1 - i] = 5;
    return !(e & 1);
}
shouldBeTrue("testPrototypeFunction('findIndex', '(keepEvenAndChange)', [11, 15, 3, 12, 44], -1, [5, 5, 5, 5, 5])");
debug("");

debug("4.0 Exception Test");
function isBigEnoughAndException(element, index, array) {
    if(index==1) throw "exception from function";
    return (element >= 10);
}
shouldBeTrue("testPrototypeFunction('findIndex', '(isBigEnoughAndException)', [12, 15, 10, 13, 44], 0)");
shouldThrow("testPrototypeFunction('findIndex', '(isBigEnoughAndException)', [9, 15, 10, 13, 44], false)");
debug("");

debug("5.0 Wrong Type for Callback Test");
shouldThrow("testPrototypeFunction('findIndex', '(8)', [12, 15, 10, 13, 44], false)", "'TypeError: TypedArray.prototype.findIndex callback must be a function'");
shouldThrow("testPrototypeFunction('findIndex', '(\"wrong\")', [12, 15, 10, 13, 44], false)", "'TypeError: TypedArray.prototype.findIndex callback must be a function'");
shouldThrow("testPrototypeFunction('findIndex', '(new Object())', [12, 15, 10, 13, 44], false)", "'TypeError: TypedArray.prototype.findIndex callback must be a function'");
shouldThrow("testPrototypeFunction('findIndex', '(null)', [12, 15, 10, 13, 44], false)", "'TypeError: TypedArray.prototype.findIndex callback must be a function'");
shouldThrow("testPrototypeFunction('findIndex', '()', [12, 15, 10, 13, 44], false)", "'TypeError: TypedArray.prototype.findIndex callback must be a function'");
debug("");
finishJSTest();
