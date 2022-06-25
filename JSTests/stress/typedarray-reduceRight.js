load("./resources/typedarray-test-helper-functions.js", "caller relative");
description(
"This test checks the behavior of the TypedArray.prototype.reduceRight function"
);

shouldBe("Int32Array.prototype.reduceRight.length", "1");
shouldBe("Int32Array.prototype.reduceRight.name", "'reduceRight'");
shouldBeTrue("isSameFunctionForEachTypedArrayPrototype('reduceRight')");
shouldBeTrue("testPrototypeReceivesArray('reduceRight', [undefined, this, { }, [ ], true, ''])");
debug("");

debug("testPrototypeFunction has the following arg list (name, args, init, result [ , expectedArray ])");
debug("");

debug("1.0 Single Argument Testing");
function createArray(acc, e, i, a) {
    if (typeof acc !== "object")
        acc = [acc];
    acc.push(e);
    return acc;
}

function sum(acc, e, i, a) { return acc + e; }

shouldBeTrue("testPrototypeFunction('reduceRight', '(createArray)', [12, 5, 8, 13, 44], [12, 5, 8, 13, 44].reverse(), [12, 5, 8, 13, 44])");
shouldBeTrue("testPrototypeFunction('reduceRight', '(sum)', [1, 2, 3, 4, 5], 15)");
debug("");

debug("2.0 Two Argument Testing");

shouldBeTrue("testPrototypeFunction('reduceRight', '(createArray, [1])', [12, 23, 11, 1, 45], [1, 45, 1, 11, 23, 12], [12, 23, 11, 1, 45])");
debug("");

debug("3.0 Array Element Changing");
function createArrayAndChange(acc, e, i, a) {
    a[a.length - 1 - i] = 5;
    acc.push(e);
    return acc;
}
shouldBeTrue("testPrototypeFunction('reduceRight', '(createArrayAndChange, [])', [12, 15, 2, 13, 44], [44, 13, 2, 5, 5], [5, 5, 5, 5, 5])");
debug("");

debug("4.0 Exception Test");
function isBigEnoughAndException(acc, element, index, array) {
    if(index==1) throw "exception from function";
    return (element >= 10);
}
shouldThrow("testPrototypeFunction('reduceRight', '(isBigEnoughAndException)', [12, 15, 10, 13, 44], false)");
debug("");

debug("5.0 Wrong Type for Callback Test");
shouldThrow("testPrototypeFunction('reduceRight', '(8)', [12, 15, 10, 13, 44], false)", "'TypeError: TypedArray.prototype.reduceRight callback must be a function'");
shouldThrow("testPrototypeFunction('reduceRight', '(\"wrong\")', [12, 15, 10, 13, 44], false)", "'TypeError: TypedArray.prototype.reduceRight callback must be a function'");
shouldThrow("testPrototypeFunction('reduceRight', '(new Object())', [12, 15, 10, 13, 44], false)", "'TypeError: TypedArray.prototype.reduceRight callback must be a function'");
shouldThrow("testPrototypeFunction('reduceRight', '(null)', [12, 15, 10, 13, 44], false)", "'TypeError: TypedArray.prototype.reduceRight callback must be a function'");
shouldThrow("testPrototypeFunction('reduceRight', '()', [12, 15, 10, 13, 44], false)", "'TypeError: TypedArray.prototype.reduceRight callback must be a function'");
shouldThrow("testPrototypeFunction('reduceRight', '(new Function())', [], false)", "'TypeError: TypedArray.prototype.reduceRight of empty array with no initial value'");
debug("");
finishJSTest();
