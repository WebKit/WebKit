description("Tests for Array.prototype.findLastIndex");

shouldBe("Array.prototype.findLastIndex.length", "1");
shouldBe("Array.prototype.findLastIndex.name", "'findLastIndex'");

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
    result.findLastIndex = Array.prototype.findLastIndex;
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
var arrayWithHoles = [];
arrayWithHoles[1] = 0;
arrayWithHoles[3] = null;
arrayWithHoles[5] = false;
arrayWithHoles[7] = "";
function numberOfCallbacksInFindIndexInArrayWithHoles() {
    var count = 0;
    arrayWithHoles.find(function(element, index, array) {
        debug("find callback called with index " + index);
        count++;
    });
    return count;
}

shouldBe("[undefined, 0, null, false, ''].findLastIndex(passUndefined)", "0");
shouldBe("[undefined, 0, null, false, ''].findLastIndex(passZero)", "1");
shouldBe("[undefined, 0, null, false, ''].findLastIndex(passNull)", "2");
shouldBe("[undefined, 0, null, false, ''].findLastIndex(passFalse)", "3");
shouldBe("[undefined, 0, null, false, ''].findLastIndex(passEmptyString)", "4");
shouldBe("[0,1,2,3,4,5,6,7,8,9].findLastIndex(passEven)", "8");
shouldBe("[0,1,2,3,4,5,6,7,8,9].findLastIndex(passAfter5)", "9");
shouldBe("[0, null, false, ''].findLastIndex(passUndefined)", "-1");
shouldBe("[undefined, 0, false, ''].findLastIndex(passNull)", "-1");
shouldBe("[undefined, 0, null, ''].findLastIndex(passFalse)", "-1");
shouldBe("[undefined, 0, null, false].findLastIndex(passEmptyString)", "-1");
shouldBe("[1,3,5,7,9].findLastIndex(passEven)", "-1");
shouldBe("[0,1,2,3,4].findLastIndex(passAfter5)", "-1");
shouldBe("[undefined, null, false, ''].findLastIndex(passZero)", "-1");

debug("Array with holes");
shouldBe("(new Array(20)).findLastIndex(passUndefined)", "19");
shouldBe("arrayWithHoles.findLastIndex(passUndefined)", "6");
shouldBe("arrayWithHoles.findLastIndex(passZero)", "1");
shouldBe("arrayWithHoles.findLastIndex(passNull)", "3");
shouldBe("arrayWithHoles.findLastIndex(passFalse)", "5");
shouldBe("arrayWithHoles.findLastIndex(passEmptyString)", "7");
shouldBe("arrayWithHoles.findLastIndex(passAfter5)", "7");
arrayWithHoles[6] = {};
shouldBe("arrayWithHoles.findLastIndex(passUndefined)", "4");

debug("Generic Object");
shouldBe("toObject([undefined, 0, null, false, '']).findLastIndex(passUndefined)", "0");
shouldBe("toObject([undefined, 0, null, false, '']).findLastIndex(passZero)", "1");
shouldBe("toObject([undefined, 0, null, false, '']).findLastIndex(passNull)", "2");
shouldBe("toObject([undefined, 0, null, false, '']).findLastIndex(passFalse)", "3");
shouldBe("toObject([undefined, 0, null, false, '']).findLastIndex(passEmptyString)", "4");
shouldBe("toObject([0, null, false, '']).findLastIndex(passUndefined)", "-1");
shouldBe("toObject([undefined, 0, false, '']).findLastIndex(passNull)", "-1");
shouldBe("toObject([undefined, 0, null, '']).findLastIndex(passFalse)", "-1");
shouldBe("toObject([undefined, 0, null, false]).findLastIndex(passEmptyString)", "-1");
shouldBe("toObject([undefined, null, false, '']).findLastIndex(passZero)", "-1");
shouldBe("toObject(new Array(20)).findLastIndex(passUndefined)", "19");

debug("Array-like object with invalid lengths");
var throwError = function throwError() {
    throw new Error("should not reach here");
};
shouldBe("var obj = { 0: 1, 1: 2, 2: 3, length: 0 }; Array.prototype.findLastIndex.call(obj, throwError)", "-1");
shouldBe("var obj = { 0: 1, 1: 2, 2: 3, length: -0 }; Array.prototype.findLastIndex.call(obj, throwError)", "-1");
shouldBe("var obj = { 0: 1, 1: 2, 2: 3, length: -3 }; Array.prototype.findLastIndex.call(obj, throwError)", "-1");

debug("Modification during search");
shouldBe("[0,1,2,3,4,5,6,7,8,9].findLastIndex(findItemAddedDuringSearch)", "-1");
shouldBe("[0,1,2,3,4,5,6,7,8,9].findLastIndex(findItemRemovedDuringSearch)", "-1");

debug("Exceptions");
shouldThrow("Array.prototype.findLastIndex.call(undefined, function() {})", "'TypeError: Array.prototype.findLastIndex requires that |this| not be null or undefined'");
shouldThrow("Array.prototype.findLastIndex.call(null, function() {})", "'TypeError: Array.prototype.findLastIndex requires that |this| not be null or undefined'");
shouldThrow("[].findLastIndex(1)", "'TypeError: Array.prototype.findLastIndex callback must be a function'");
shouldThrow("[].findLastIndex('hello')", "'TypeError: Array.prototype.findLastIndex callback must be a function'");
shouldThrow("[].findLastIndex([])", "'TypeError: Array.prototype.findLastIndex callback must be a function'");
shouldThrow("[].findLastIndex({})", "'TypeError: Array.prototype.findLastIndex callback must be a function'");
shouldThrow("[].findLastIndex(null)", "'TypeError: Array.prototype.findLastIndex callback must be a function'");
shouldThrow("[].findLastIndex(undefined)", "'TypeError: Array.prototype.findLastIndex callback must be a function'");

debug("Callbacks in the expected order and *not* skipping holes");
shouldBe("numberOfCallbacksInFindIndexInArrayWithHoles()", "8");
