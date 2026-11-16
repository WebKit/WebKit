function test(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

test(Array.prototype[Symbol.iterator], Array.prototype.values);
test(Map.prototype[Symbol.iterator], Map.prototype.entries);
test(Set.prototype[Symbol.iterator], Set.prototype.values);

function argumentsTests(values) {
    test(function () {
        return arguments[Symbol.iterator];
    }(), values);

    test(function (a, b, c) {
        return arguments[Symbol.iterator];
    }(), values);

    test(function () {
        'use strict';
        return arguments[Symbol.iterator];
    }(), values);

    test(function (a, b, c) {
        'use strict';
        return arguments[Symbol.iterator];
    }(), values);
}

argumentsTests(Array.prototype.values);
var arrayValues = Array.prototype.values;
Array.prototype.values = null;
argumentsTests(arrayValues);
