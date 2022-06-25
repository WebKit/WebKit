description(
"This test checks the behavior of the every() method on Array objects."
);

shouldBe("Array.prototype.every.length", "1");
shouldBe("Array.prototype.every.name", "'every'");
debug("");

debug("1.0 Single Argument Testing");
function isBigEnough(element, index, array) {
    return (element >= 10);
}
shouldBeFalse("[12, 5, 8, 130, 44].every(isBigEnough)");
shouldBeTrue("[12, 54, 18, 130, 44].every(isBigEnough)");
debug("");

debug("2.0 Two Argument Testing");
var predicate = {
    comparison:     11,
    isBigEnough:    function(s) {
                        return (s >= comparison);
                    }
};
shouldBeFalse("[12, 5, 10, 130, 44].every(isBigEnough, predicate)");
shouldBeTrue("[12, 54, 18, 130, 44].every(isBigEnough, predicate)");
debug("");

debug("3.0 Array Mutation Tests");
debug("");

debug("3.1 Array Element Removal");
function isBigEnoughAndPop(element, index, array) {
    array.pop();
    return (element >= 10);
}
shouldBeFalse("[12, 5, 8, 130, 44].every(isBigEnoughAndPop)");
shouldBeTrue("[12, 54, 18, 130, 44].every(isBigEnoughAndPop)");
debug("");

debug("3.2 Array Element Changing");
function isBigEnoughAndChange(element, index, array) {
    array[array.length-1-index]= 5;
    return (element >= 10);
}
shouldBeFalse("[12, 5, 8, 130, 44].every(isBigEnoughAndChange)");
shouldBeFalse("[12, 54, 18, 130, 44].every(isBigEnoughAndChange)");
debug("");

debug("3.3 Array Element Addition");
function isBigEnoughAndPush(element, index, array) {
    array.push(131);
    return (element >= 131);
}
shouldBeFalse("[12, 5, 8, 130, 44].every(isBigEnoughAndPush)");
shouldBeFalse("[12, 54, 18, 130, 44].every(isBigEnoughAndPush)");
debug("");

debug("4.0 Exception Test");
function isBigEnoughAndException(element, index, array) {
    if(index==1) throw "exception from function";
    return (element >= 10);
}
shouldThrow("[12, 5, 8, 130, 44].every(isBigEnoughAndException)", '"exception from function"');
shouldThrow("[12, 54, 18, 130, 44].every(isBigEnoughAndException)", '"exception from function"');
debug("");

debug("5.0 Wrong Type for Callback Test");
shouldThrow("[12, 5, 8, 130, 44].every(5)");
shouldThrow("[12, 5, 8, 130, 44].every('wrong')");
shouldThrow("[12, 5, 8, 130, 44].every(new Object())");
shouldThrow("[12, 5, 8, 130, 44].every(null)");
shouldThrow("[12, 5, 8, 130, 44].every(undefined)");
shouldThrow("[12, 5, 8, 130, 44].every()");
debug("");

debug('6.0 Early Exit ("Short Circuiting")');
var accumulator = new Array();
function isBigEnoughShortCircuit(element, index, array) {
    accumulator.push(element);
    return (element >= 10);
}
shouldBeFalse("[12, 5, 8, 130, 44].every(isBigEnoughShortCircuit)");
shouldBe("accumulator.toString()", "[12, 5].toString()");
accumulator.length = 0;
shouldBeTrue("[12, 54, 18, 130, 44].every(isBigEnoughShortCircuit)");
shouldBe("accumulator.toString()", "[12, 54, 18, 130, 44].toString()");
debug("");

debug('7.0 Behavior for Holes in Arrays');
var arr = [5, 5, 5, 5];
delete arr[1];
function isNotUndefined(element, index, array) {
    return typeof element !== "undefined";
}
shouldBeTrue("arr.every(isNotUndefined)");
arr = new Array(20);
shouldBeTrue("arr.every(isNotUndefined)");
debug("");

debug("8.0 Array-like object with invalid lengths");
var throwError = function throwError() {
    throw new Error("should not reach here");
};
shouldBeTrue("var obj = { 0: 1, 1: 2, 2: 3, length: 0 }; Array.prototype.every.call(obj, throwError)");
shouldBeTrue("var obj = { 0: 1, 1: 2, 2: 3, length: -0 }; Array.prototype.every.call(obj, throwError)");
shouldBeTrue("var obj = { 0: 1, 1: 2, 2: 3, length: -3 }; Array.prototype.every.call(obj, throwError)");
debug("");
