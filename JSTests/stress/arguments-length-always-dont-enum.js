function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}


var argsSloppy = (function () { return arguments; }(1,2,3));
var argsStrict = (function () { 'use strict'; return arguments; }(1,2,3));

shouldBe(Object.prototype.propertyIsEnumerable(argsSloppy, 'length'), false);
shouldBe(Object.prototype.propertyIsEnumerable(argsStrict, 'length'), false);

shouldBe(Object.keys(argsSloppy).length === Object.keys(argsStrict).length, true);
shouldBe(Object.keys(argsSloppy).indexOf('length'), -1)
shouldBe(Object.keys(argsStrict).indexOf('length'), -1);
