function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function testFunction(func) {
    var array = Object.getOwnPropertyNames(func);
    shouldBe(array.indexOf("arguments"), -1);
    shouldBe(array.indexOf("caller"), -1);
}

testFunction((() => { }).bind());
testFunction((() => { "use strict"; }).bind());
testFunction((function () { }).bind());
testFunction((function () { "use strict"; }).bind());
