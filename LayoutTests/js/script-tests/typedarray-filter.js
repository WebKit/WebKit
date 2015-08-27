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
shouldThrow("testPrototypeFunction('filter', '(8)', [12, 15, 10, 13, 44], false)");
shouldThrow("testPrototypeFunction('filter', '(\"wrong\")', [12, 15, 10, 13, 44], false)");
shouldThrow("testPrototypeFunction('filter', '(new Object())', [12, 15, 10, 13, 44], false)");
shouldThrow("testPrototypeFunction('filter', '(null)', [12, 15, 10, 13, 44], false)");
shouldThrow("testPrototypeFunction('filter', '()', [12, 15, 10, 13, 44], false)");
debug("");
