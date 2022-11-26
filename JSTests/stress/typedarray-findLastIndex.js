load("./resources/typedarray-test-helper-functions.js", "caller relative");
description(
"This test checks the behavior of the TypedArray.prototype.findLastIndex function"
);

shouldBe("Int32Array.prototype.findLastIndex.length", "1");
shouldBe("Int32Array.prototype.findLastIndex.name", "'findLastIndex'");
shouldBeTrue("isSameFunctionForEachTypedArrayPrototype('findLastIndex')");
shouldBeTrue("testPrototypeReceivesArray('findLastIndex', [undefined, this, { }, [ ], true, ''])");
debug("");

debug("testPrototypeFunction has the following arg list (name, args, init, result [ , expectedArray ])");
debug("");

debug("1.0 Single Argument Testing");
function keepEven(e, i) {
    return !(e & 1) || (this.keep ? this.keep === i : false);
}
shouldBeTrue("testPrototypeFunction('findLastIndex', '(keepEven)', [12, 5, 8, 13, 44], 4)");
shouldBeTrue("testPrototypeFunction('findLastIndex', '(keepEven)', [11, 13, 17, 13, 22], 4)");
shouldBeTrue("testPrototypeFunction('findLastIndex', '(keepEven)', [22, 13, 17, 13, 11], 0)");
shouldBeTrue("testPrototypeFunction('findLastIndex', '(keepEven)', [11, 13, 17, 13, 11], -1)");
debug("");

debug("2.0 Two Argument Testing");
var thisValue = { keep: 3 };
shouldBeTrue("testPrototypeFunction('findLastIndex', '(keepEven, thisValue)', [11, 23, 11, 1, 44], 4)");
shouldBeTrue("testPrototypeFunction('findLastIndex', '(keepEven, thisValue)', [11, 23, 11, 1, 43], 3)");
debug("");

debug("3.0 Array Element Changing");
function keepEvenAndChange(e, i, a) {
    a[a.length - 1 - i] = 5;
    return !(e & 1);
}
shouldBeTrue("testPrototypeFunction('findLastIndex', '(keepEvenAndChange)', [11, 15, 3, 12, 44], 4, [5, 15, 3, 12, 44])");
shouldBeTrue("testPrototypeFunction('findLastIndex', '(keepEvenAndChange)', [44, 12, 3, 15, 11], -1, [5, 5, 5, 5, 5])");
debug("");

debug("4.0 Exception Test");
function isBigEnoughAndException(element, index, array) {
    if(index==3) throw "exception from function";
    return (element >= 44);
}
shouldBeTrue("testPrototypeFunction('findLastIndex', '(isBigEnoughAndException)', [12, 15, 10, 13, 44], 4)");
shouldThrow("testPrototypeFunction('findLastIndex', '(isBigEnoughAndException)', [9, 15, 10, 13, 43], false)");
debug("");

debug("5.0 Wrong Type for Callback Test");
shouldThrow("testPrototypeFunction('findLastIndex', '(8)', [12, 15, 10, 13, 44], false)", "'TypeError: TypedArray.prototype.findLastIndex callback must be a function'");
shouldThrow("testPrototypeFunction('findLastIndex', '(\"wrong\")', [12, 15, 10, 13, 44], false)", "'TypeError: TypedArray.prototype.findLastIndex callback must be a function'");
shouldThrow("testPrototypeFunction('findLastIndex', '(new Object())', [12, 15, 10, 13, 44], false)", "'TypeError: TypedArray.prototype.findLastIndex callback must be a function'");
shouldThrow("testPrototypeFunction('findLastIndex', '(null)', [12, 15, 10, 13, 44], false)", "'TypeError: TypedArray.prototype.findLastIndex callback must be a function'");
shouldThrow("testPrototypeFunction('findLastIndex', '()', [12, 15, 10, 13, 44], false)", "'TypeError: TypedArray.prototype.findLastIndex callback must be a function'");
debug("");
finishJSTest();
