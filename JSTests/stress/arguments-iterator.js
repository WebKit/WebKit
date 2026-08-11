function test(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function testArguments(check) {
    (function () {
        check(arguments, []);
    }());

    (function (a, b, c) {
        check(arguments, [a, b, c]);
    }());

    (function () {
        'use strict';
        check(arguments, []);
    }());

    (function (a, b, c) {
        'use strict';
        check(arguments, [a, b, c]);
    }());
}

testArguments(function (args) {
    var iteratorMethod = args[Symbol.iterator];
    test(iteratorMethod, Array.prototype.values);
    var descriptor = Object.getOwnPropertyDescriptor(args, Symbol.iterator);
    test(descriptor.writable, true);
    test(descriptor.configurable, true);
    test(descriptor.enumerable, false);
    test(descriptor.value, iteratorMethod);
});

testArguments(function (args, expected) {
    var iterator = args[Symbol.iterator]();
    test(iterator.toString(), '[object Array Iterator]');
    var index = 0;
    for (var value of iterator) {
        test(value, expected[index++]);
    }
    test(args.length, index);

    var index = 0;
    for (var value of args) {
        test(value, expected[index++]);
    }
    test(args.length, index);
});

testArguments(function (args) {
    var symbols = Object.getOwnPropertySymbols(args);
    test(symbols.length, 1);
    test(symbols[0], Symbol.iterator);
});

testArguments(function (args) {
    'use strict';
    args[Symbol.iterator] = 'not throw error';
});

testArguments(function (args) {
    'use strict';
    delete args[Symbol.iterator];
    test(args[Symbol.iterator], undefined);

    var symbols = Object.getOwnPropertySymbols(args);
    test(symbols.length, 0);
});
