function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

// Set is iterable.
var set = new Set([0, 1, 2, 3, 4, 5]);
var array = Array.from(set);

shouldBe(array.length, set.size);
for (var i = 0; i < array.length; ++i) {
    shouldBe(set.has(array[i]), true);
}

// Map is iterable.
var map = new Map([
    [0, 0],
    [1, 0],
    [2, 0],
    [3, 0],
    [4, 0],
    [5, 0]
]);
var array = Array.from(map);

shouldBe(array.length, map.size);
for (var i = 0; i < array.length; ++i) {
    shouldBe(array[i][1], 0);
    shouldBe(map.has(array[i][0]), true);
    shouldBe(map.get(array[i][0]), 0);
}

// String is iterable
var string = "Cocoa Cappuccino";
var array = Array.from(string);
shouldBe(array.length, string.length);
for (var i = 0; i < array.length; ++i) {
    shouldBe(array[i], string[i]);
}

// Arguments is iterable
var argumentsGenerators = [
    function () {
        return arguments;
    },

    function () {
        'use strict';
        return arguments;
    },

    function (a, b, c) {
        return arguments;
    },

    function (a, b, c) {
        'use strict';
        return arguments;
    }
];

for (var gen of argumentsGenerators) {
    var args = gen(1, 2, 3, 4, 5);
    var array = Array.from(args);
    shouldBe(array.length, args.length);
    for (var i = 0; i < array.length; ++i) {
        shouldBe(array[i], args[i]);
    }
}
