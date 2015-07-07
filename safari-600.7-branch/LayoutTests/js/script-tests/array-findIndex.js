description("Tests for Array.prototype.findIndex");

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
    result.findIndex=Array.prototype.findIndex;
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
arrayWithHoles[10] = 0;
arrayWithHoles[20] = null;
arrayWithHoles[30] = false;
arrayWithHoles[40] = "";
function numberOfCallbacksInFindIndexInArrayWithHoles() {
    var count = 0;
    arrayWithHoles.find(function(element, index, array) {
        debug("find callback called with index " + index);
        count++;
    });
    return count;
}

shouldBe("[undefined, 0, null, false, ''].findIndex(passUndefined)", "0");
shouldBe("[undefined, 0, null, false, ''].findIndex(passZero)", "1");
shouldBe("[undefined, 0, null, false, ''].findIndex(passNull)", "2");
shouldBe("[undefined, 0, null, false, ''].findIndex(passFalse)", "3");
shouldBe("[undefined, 0, null, false, ''].findIndex(passEmptyString)", "4");
shouldBe("[0, null, false, ''].findIndex(passUndefined)", "-1");
shouldBe("[undefined, 0, false, ''].findIndex(passNull)", "-1");
shouldBe("[undefined, 0, null, ''].findIndex(passFalse)", "-1");
shouldBe("[undefined, 0, null, false].findIndex(passEmptyString)", "-1");
shouldBe("[undefined, null, false, ''].findIndex(passZero)", "-1");
shouldBe("(new Array(20)).findIndex(passUndefined)", "-1");

// Array with holes.
shouldBe("arrayWithHoles.findIndex(passUndefined)", "-1");
shouldBe("arrayWithHoles.findIndex(passZero)", "10");
shouldBe("arrayWithHoles.findIndex(passNull)", "20");
shouldBe("arrayWithHoles.findIndex(passFalse)", "30");
shouldBe("arrayWithHoles.findIndex(passEmptyString)", "40");
arrayWithHoles[50] = undefined;
shouldBe("arrayWithHoles.findIndex(passUndefined)", "50");

// Generic Object
shouldBe("toObject([undefined, 0, null, false, '']).findIndex(passUndefined)", "0");
shouldBe("toObject([undefined, 0, null, false, '']).findIndex(passZero)", "1");
shouldBe("toObject([undefined, 0, null, false, '']).findIndex(passNull)", "2");
shouldBe("toObject([undefined, 0, null, false, '']).findIndex(passFalse)", "3");
shouldBe("toObject([undefined, 0, null, false, '']).findIndex(passEmptyString)", "4");
shouldBe("toObject([0, null, false, '']).findIndex(passUndefined)", "-1");
shouldBe("toObject([undefined, 0, false, '']).findIndex(passNull)", "-1");
shouldBe("toObject([undefined, 0, null, '']).findIndex(passFalse)", "-1");
shouldBe("toObject([undefined, 0, null, false]).findIndex(passEmptyString)", "-1");
shouldBe("toObject([undefined, null, false, '']).findIndex(passZero)", "-1");
shouldBe("toObject(new Array(20)).findIndex(passUndefined)", "-1");

// Modification during search
shouldBe("[0,1,2,3,4,5,6,7,8,9].findIndex(findItemAddedDuringSearch)", "-1");
shouldBe("[0,1,2,3,4,5,6,7,8,9].findIndex(findItemRemovedDuringSearch)", "-1");

// Exeptions
shouldThrow("Array.prototype.findIndex.call(undefined, function() {})", "'TypeError: Array.prototype.findIndex requires that |this| not be undefined'");
shouldThrow("Array.prototype.findIndex.call(null, function() {})", "'TypeError: Array.prototype.findIndex requires that |this| not be null'");
shouldThrow("[].findIndex(1)", "'TypeError: Array.prototype.findIndex callback must be a function'");
shouldThrow("[].findIndex('hello')", "'TypeError: Array.prototype.findIndex callback must be a function'");
shouldThrow("[].findIndex([])", "'TypeError: Array.prototype.findIndex callback must be a function'");
shouldThrow("[].findIndex({})", "'TypeError: Array.prototype.findIndex callback must be a function'");
shouldThrow("[].findIndex(null)", "'TypeError: Array.prototype.findIndex callback must be a function'");
shouldThrow("[].findIndex(undefined)", "'TypeError: Array.prototype.findIndex callback must be a function'");

// Callbacks in the expected order and skipping holes.
shouldBe("numberOfCallbacksInFindIndexInArrayWithHoles()", "5");
