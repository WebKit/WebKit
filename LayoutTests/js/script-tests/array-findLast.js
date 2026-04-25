description("Tests for Array.prototype.findLast");

shouldBe("Array.prototype.findLast.length", "1");
shouldBe("Array.prototype.findLast.name", "'findLast'");

function passUndefined(element, index, array) {
    return typeof element === "undefined";
}
function passZero(element, index, array) {
    return element === 0;
}
function passNull(element, index, array) {
    return element === null;
}
function passFalse(element, index, array) {
    return element === false;
}
function passEmptyString(element, index, array) {
    return element === "";
}
function passEven(a) {
    return !(a & 1);
}
function passAfter5(element, index) {
    return index >= 5;
}
function toObject(array) {
    var result = {};
    result.length = array.length;
    for (var i in array)
        result[i] = array[i];
    result.findLast=Array.prototype.findLast;
    return result;
}
function findItemAddedDuringSearch(element, index, array) {
    if (index === array.length - 1)
        array.unshift(array.length);
    return (index === array.length - 1);
}
function findItemRemovedDuringSearch(element, index, array) {
    if (index === 0)
        array.shift();
    return (index === 0 && array[0] === element);
}
arrayWithHoles = [];
arrayWithHoles[1] = 0;
arrayWithHoles[3] = null;
arrayWithHoles[5] = false;
arrayWithHoles[7] = "";
function numberOfCallbacksInFindInArrayWithHoles() {
    var count = 0;
    arrayWithHoles.findLast(function(element, index, array) {
        debug("findLast callback called with index " + index);
        count++;
    });
    return count;
}

shouldBe("[undefined, 0, null, false, ''].findLast(passUndefined)", "undefined");
shouldBe("[undefined, 0, null, false, ''].findLast(passZero)", "0");
shouldBe("[undefined, 0, null, false, ''].findLast(passNull)", "null");
shouldBe("[undefined, 0, null, false, ''].findLast(passFalse)", "false");
shouldBe("[undefined, 0, null, false, ''].findLast(passEmptyString)", "''");
shouldBe("[0,1,2,3,4,5,6,7,8,9].findLast(passEven)", "8");
shouldBe("[0,1,2,3,4,5,6,7,8,9].findLast(passAfter5)", "9");
shouldBe("[0, null, false, ''].findLast(passUndefined)", "undefined");
shouldBe("[undefined, 0, false, ''].findLast(passNull)", "undefined");
shouldBe("[undefined, 0, null, ''].findLast(passFalse)", "undefined");
shouldBe("[undefined, 0, null, false].findLast(passEmptyString)", "undefined");
shouldBe("[1,3,5,7,9].findLast(passEven)", "undefined");
shouldBe("[0,1,2,3,4].findLast(passAfter5)", "undefined");
shouldBe("[undefined, null, false, ''].findLast(passZero)", "undefined");

debug("Array with holes")
shouldBe("(new Array(20)).findLast(passUndefined)", "undefined");
shouldBe("arrayWithHoles.findLast(passUndefined)", "undefined");
shouldBe("arrayWithHoles.findLast(passZero)", "0");
shouldBe("arrayWithHoles.findLast(passNull)", "null");
shouldBe("arrayWithHoles.findLast(passFalse)", "false");
shouldBe("arrayWithHoles.findLast(passEmptyString)", "''");
shouldBe("arrayWithHoles.findLast(passAfter5)", "''");

debug("Generic Object");
shouldBe("toObject([undefined, 0, null, false, '']).findLast(passUndefined)", "undefined");
shouldBe("toObject([undefined, 0, null, false, '']).findLast(passZero)", "0");
shouldBe("toObject([undefined, 0, null, false, '']).findLast(passNull)", "null");
shouldBe("toObject([undefined, 0, null, false, '']).findLast(passFalse)", "false");
shouldBe("toObject([undefined, 0, null, false, '']).findLast(passEmptyString)", "''");
shouldBe("toObject([0, null, false, '']).findLast(passUndefined)", "undefined");
shouldBe("toObject([undefined, 0, false, '']).findLast(passNull)", "undefined");
shouldBe("toObject([undefined, 0, null, '']).findLast(passFalse)", "undefined");
shouldBe("toObject([undefined, 0, null, false]).findLast(passEmptyString)", "undefined");
shouldBe("toObject([undefined, null, false, '']).findLast(passZero)", "undefined");
shouldBe("toObject(new Array(20)).findLast(passUndefined)", "undefined");

debug("Array-like object with invalid lengths");
var throwError = function throwError() {
    throw new Error("should not reach here");
};
shouldBeUndefined("var obj = { 0: 1, 1: 2, 2: 3, length: 0 }; Array.prototype.findLast.call(obj, throwError)");
shouldBeUndefined("var obj = { 0: 1, 1: 2, 2: 3, length: -0 }; Array.prototype.findLast.call(obj, throwError)");
shouldBeUndefined("var obj = { 0: 1, 1: 2, 2: 3, length: -3 }; Array.prototype.findLast.call(obj, throwError)");

debug("Modification during search");
shouldBe("[0,1,2,3,4,5,6,7,8,9].findLast(findItemAddedDuringSearch)", "undefined");
shouldBe("[0,1,2,3,4,5,6,7,8,9].findLast(findItemRemovedDuringSearch)", "undefined");

debug("Exceptions");
shouldThrow("Array.prototype.findLast.call(undefined, function() {})", "'TypeError: Array.prototype.findLast requires that |this| not be null or undefined'");
shouldThrow("Array.prototype.findLast.call(null, function() {})", "'TypeError: Array.prototype.findLast requires that |this| not be null or undefined'");
shouldThrow("[].findLast(1)", "'TypeError: Array.prototype.findLast callback must be a function'");
shouldThrow("[].findLast('hello')", "'TypeError: Array.prototype.findLast callback must be a function'");
shouldThrow("[].findLast([])", "'TypeError: Array.prototype.findLast callback must be a function'");
shouldThrow("[].findLast({})", "'TypeError: Array.prototype.findLast callback must be a function'");
shouldThrow("[].findLast(null)", "'TypeError: Array.prototype.findLast callback must be a function'");
shouldThrow("[].findLast(undefined)", "'TypeError: Array.prototype.findLast callback must be a function'");

debug("Callbacks in the expected order and *not* skipping holes");
shouldBe("numberOfCallbacksInFindInArrayWithHoles()", "8");
