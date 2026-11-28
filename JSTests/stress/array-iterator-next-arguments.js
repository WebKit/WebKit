function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' expected: ' + expected);
}

function sloppySum() {
    var result = 0;
    for (var value of arguments)
        result += value;
    return result;
}
noInline(sloppySum);

function strictSum() {
    "use strict";
    var result = 0;
    for (var value of arguments)
        result += value;
    return result;
}
noInline(strictSum);

function scopedSum(a, b) {
    var result = 0;
    for (var value of arguments)
        result += value;
    return result + (() => a + b)();
}
noInline(scopedSum);

function sloppyEntries() {
    var result = '';
    for (var [index, value] of Array.prototype.entries.call(arguments))
        result += index + ':' + value + ',';
    return result;
}
noInline(sloppyEntries);

function strictManual() {
    "use strict";
    var iterator = arguments[Symbol.iterator]();
    var result = [];
    var step;
    while (!(step = iterator.next()).done)
        result.push(step.value);
    return result.join(',');
}
noInline(strictManual);

function sloppyOverriddenLength() {
    arguments.length = 2;
    var result = [];
    for (var value of arguments)
        result.push(value);
    return result.join(',');
}
noInline(sloppyOverriddenLength);

function strictOverriddenLength() {
    "use strict";
    arguments.length = 4;
    var result = [];
    for (var value of arguments)
        result.push(value);
    return result.join(',');
}
noInline(strictOverriddenLength);

function strictNonIntLength() {
    "use strict";
    arguments.length = "2";
    var result = [];
    for (var value of arguments)
        result.push(value);
    return result.join(',');
}
noInline(strictNonIntLength);

function sloppyDeleted() {
    delete arguments[1];
    var result = [];
    for (var value of arguments)
        result.push(value);
    return result.join(',');
}
noInline(sloppyDeleted);

for (var i = 0; i < testLoopCount; ++i) {
    shouldBe(sloppySum(i, 1, 2), i + 3);
    shouldBe(strictSum(i, 1, 2), i + 3);
    shouldBe(scopedSum(1, 2, 3), 9);
    shouldBe(sloppySum(), 0);
    shouldBe(strictSum(), 0);
    shouldBe(sloppyEntries('a', 'b'), '0:a,1:b,');
    shouldBe(strictManual(1, 2, 3), '1,2,3');
    shouldBe(sloppyOverriddenLength(1, 2, 3), '1,2');
    shouldBe(strictOverriddenLength(1, 2), '1,2,,');
    shouldBe(strictNonIntLength(1, 2, 3), '1,2');
    shouldBe(sloppyDeleted(1, 2, 3), '1,,3');
}
