load("./resources/typedarray-test-helper-functions.js", "caller relative");
description(
"This test checks the behavior of the TypedArray.prototype.some function"
);

shouldBe("Int32Array.prototype.some.length", "1");
shouldBe("Int32Array.prototype.some.name", "'some'");
shouldBeTrue("isSameFunctionForEachTypedArrayPrototype('some')");
shouldBeTrue("testPrototypeReceivesArray('some', [undefined, this, { }, [ ], true, ''])");
debug("");

debug("testPrototypeFunction has the following arg list (name, args, init, result [ , expectedArray ])");
debug("");

debug("1.0 Single Argument Testing");
function isBigEnough(element, index, array) {
    if (this.value)
        return element >= this.value;
    return element >= 10;
}
shouldBeTrue("testPrototypeFunction('some', '(isBigEnough)', [12, 5, 8, 13, 44], true, [12, 5, 8, 13, 44])");
shouldBeTrue("testPrototypeFunction('some', '(isBigEnough)', [2, 4, 8, 3, 4], false)");
debug("");

debug("2.0 Two Argument Testing");
var thisValue = { value: 11 };
shouldBeTrue("testPrototypeFunction('some', '(isBigEnough, thisValue)', [2, 5, 10, 3, 4], false)");
shouldBeTrue("testPrototypeFunction('some', '(isBigEnough, thisValue)', [12, 54, 82, 13, 44], true)");
debug("");

debug("3.0 Array Element Changing");
function isBigEnoughAndChange(element, index, array) {
    array[array.length - 1 - index] = 5;
    return (element >= 10);
}
shouldBeTrue("testPrototypeFunction('some', '(isBigEnoughAndChange)', [2, 5, 1, 13, 44], false, [5, 5, 5, 5, 5])");
shouldBeTrue("testPrototypeFunction('some', '(isBigEnoughAndChange)', [12, 15, 10, 13, 44], true, [12, 15, 10, 13, 5])");
debug("");

debug("4.0 Exception Test");
function isBigEnoughAndException(element, index, array) {
    if(index==1) throw "exception from function";
    return (element >= 10);
}
shouldBeTrue("testPrototypeFunction('some', '(isBigEnoughAndException)', [12, 15, 10, 13, 44], true)");
shouldThrow("testPrototypeFunction('some', '(isBigEnoughAndException)', [1, 15, 10, 13, 44], false)");
debug("");

debug("5.0 Wrong Type for Callback Test");
shouldThrow("testPrototypeFunction('some', '(8)', [12, 15, 10, 13, 44], false)", "'TypeError: TypedArray.prototype.some callback must be a function'");
shouldThrow("testPrototypeFunction('some', '(\"wrong\")', [12, 15, 10, 13, 44], false)", "'TypeError: TypedArray.prototype.some callback must be a function'");
shouldThrow("testPrototypeFunction('some', '(new Object())', [12, 15, 10, 13, 44], false)", "'TypeError: TypedArray.prototype.some callback must be a function'");
shouldThrow("testPrototypeFunction('some', '(null)', [12, 15, 10, 13, 44], false)", "'TypeError: TypedArray.prototype.some callback must be a function'");
shouldThrow("testPrototypeFunction('some', '()', [12, 15, 10, 13, 44], false)", "'TypeError: TypedArray.prototype.some callback must be a function'");
debug("");
finishJSTest();
