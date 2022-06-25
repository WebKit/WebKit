description("Tests for Array.prototype.find");

shouldBe("Array.prototype.find.length", "1");
shouldBe("Array.prototype.find.name", "'find'");

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
    result.find=Array.prototype.find;
    return result;
}
function findItemAddedDuringSearch(element, index, array) {
    if (index === 0)
        array.push(array.length);
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
    arrayWithHoles.find(function(element, index, array) {
        debug("find callback called with index " + index);
        count++;
    });
    return count;
}

shouldBe("[undefined, 0, null, false, ''].find(passUndefined)", "undefined");
shouldBe("[undefined, 0, null, false, ''].find(passZero)", "0");
shouldBe("[undefined, 0, null, false, ''].find(passNull)", "null");
shouldBe("[undefined, 0, null, false, ''].find(passFalse)", "false");
shouldBe("[undefined, 0, null, false, ''].find(passEmptyString)", "''");
shouldBe("[0, null, false, ''].find(passUndefined)", "undefined");
shouldBe("[undefined, 0, false, ''].find(passNull)", "undefined");
shouldBe("[undefined, 0, null, ''].find(passFalse)", "undefined");
shouldBe("[undefined, 0, null, false].find(passEmptyString)", "undefined");
shouldBe("[undefined, null, false, ''].find(passZero)", "undefined");

debug("Array with holes")
shouldBe("(new Array(20)).find(passUndefined)", "undefined");
shouldBe("arrayWithHoles.find(passUndefined)", "undefined");
shouldBe("arrayWithHoles.find(passZero)", "0");
shouldBe("arrayWithHoles.find(passNull)", "null");
shouldBe("arrayWithHoles.find(passFalse)", "false");
shouldBe("arrayWithHoles.find(passEmptyString)", "''");

debug("Generic Object");
shouldBe("toObject([undefined, 0, null, false, '']).find(passUndefined)", "undefined");
shouldBe("toObject([undefined, 0, null, false, '']).find(passZero)", "0");
shouldBe("toObject([undefined, 0, null, false, '']).find(passNull)", "null");
shouldBe("toObject([undefined, 0, null, false, '']).find(passFalse)", "false");
shouldBe("toObject([undefined, 0, null, false, '']).find(passEmptyString)", "''");
shouldBe("toObject([0, null, false, '']).find(passUndefined)", "undefined");
shouldBe("toObject([undefined, 0, false, '']).find(passNull)", "undefined");
shouldBe("toObject([undefined, 0, null, '']).find(passFalse)", "undefined");
shouldBe("toObject([undefined, 0, null, false]).find(passEmptyString)", "undefined");
shouldBe("toObject([undefined, null, false, '']).find(passZero)", "undefined");
shouldBe("toObject(new Array(20)).find(passUndefined)", "undefined");

debug("Array-like object with invalid lengths");
var throwError = function throwError() {
    throw new Error("should not reach here");
};
shouldBeUndefined("var obj = { 0: 1, 1: 2, 2: 3, length: 0 }; Array.prototype.find.call(obj, throwError)");
shouldBeUndefined("var obj = { 0: 1, 1: 2, 2: 3, length: -0 }; Array.prototype.find.call(obj, throwError)");
shouldBeUndefined("var obj = { 0: 1, 1: 2, 2: 3, length: -3 }; Array.prototype.find.call(obj, throwError)");

debug("Modification during search");
shouldBe("[0,1,2,3,4,5,6,7,8,9].find(findItemAddedDuringSearch)", "undefined");
shouldBe("[0,1,2,3,4,5,6,7,8,9].find(findItemRemovedDuringSearch)", "undefined");

debug("Exceptions");
shouldThrow("Array.prototype.find.call(undefined, function() {})", "'TypeError: Array.prototype.find requires that |this| not be null or undefined'");
shouldThrow("Array.prototype.find.call(null, function() {})", "'TypeError: Array.prototype.find requires that |this| not be null or undefined'");
shouldThrow("[].find(1)", "'TypeError: Array.prototype.find callback must be a function'");
shouldThrow("[].find('hello')", "'TypeError: Array.prototype.find callback must be a function'");
shouldThrow("[].find([])", "'TypeError: Array.prototype.find callback must be a function'");
shouldThrow("[].find({})", "'TypeError: Array.prototype.find callback must be a function'");
shouldThrow("[].find(null)", "'TypeError: Array.prototype.find callback must be a function'");
shouldThrow("[].find(undefined)", "'TypeError: Array.prototype.find callback must be a function'");

debug("Callbacks in the expected order and *not* skipping holes");
shouldBe("numberOfCallbacksInFindInArrayWithHoles()", "8");
