load("./resources/typedarray-test-helper-functions.js", "caller relative");
description(
"This test checks the behavior of the TypedArray.prototype.find function"
);

shouldBe("Int32Array.prototype.find.length", "1");
shouldBe("Int32Array.prototype.find.name", "'find'");
shouldBeTrue("isSameFunctionForEachTypedArrayPrototype('find')");
shouldBeTrue("testPrototypeReceivesArray('find', [undefined, this, { }, [ ], true, ''])");
debug("");

debug("testPrototypeFunction has the following arg list (name, args, init, result [ , expectedArray ])");
debug("");

debug("1.0 Single Argument Testing");
function keepEven(e, i) {
    return !(e & 1) || (this.keep ? this.keep === i : false);
}
shouldBeTrue("testPrototypeFunction('find', '(keepEven)', [12, 5, 8, 13, 44], 12)");
shouldBeTrue("testPrototypeFunction('find', '(keepEven)', [11, 13, 17, 13, 22], 22)");
shouldBeTrue("testPrototypeFunction('find', '(keepEven)', [11, 13, 17, 13, 11], undefined)");
debug("");

debug("2.0 Two Argument Testing");
var thisValue = { keep: 3 };
shouldBeTrue("testPrototypeFunction('find', '(keepEven, thisValue)', [11, 23, 11, 1, 44], 1)");
debug("");

debug("3.0 Array Element Changing");
function keepEvenAndChange(e, i, a) {
    a[a.length - 1 - i] = 5;
    return !(e & 1);
}
shouldBeTrue("testPrototypeFunction('find', '(keepEvenAndChange)', [11, 15, 3, 12, 44], undefined, [5, 5, 5, 5, 5])");
debug("");

debug("4.0 Exception Test");
function isBigEnoughAndException(element, index, array) {
    if(index==1) throw "exception from function";
    return (element >= 10);
}
shouldBeTrue("testPrototypeFunction('find', '(isBigEnoughAndException)', [12, 15, 10, 13, 44], 12)");
shouldThrow("testPrototypeFunction('find', '(isBigEnoughAndException)', [9, 15, 10, 13, 44], false)");
debug("");

debug("5.0 Wrong Type for Callback Test");
shouldThrow("testPrototypeFunction('find', '(8)', [12, 15, 10, 13, 44], false)", "'TypeError: TypedArray.prototype.find callback must be a function'");
shouldThrow("testPrototypeFunction('find', '(\"wrong\")', [12, 15, 10, 13, 44], false)", "'TypeError: TypedArray.prototype.find callback must be a function'");
shouldThrow("testPrototypeFunction('find', '(new Object())', [12, 15, 10, 13, 44], false)", "'TypeError: TypedArray.prototype.find callback must be a function'");
shouldThrow("testPrototypeFunction('find', '(null)', [12, 15, 10, 13, 44], false)", "'TypeError: TypedArray.prototype.find callback must be a function'");
shouldThrow("testPrototypeFunction('find', '()', [12, 15, 10, 13, 44], false)", "'TypeError: TypedArray.prototype.find callback must be a function'");
debug("");
finishJSTest();
