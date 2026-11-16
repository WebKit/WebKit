function test(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

(function () {
    var array = [];
    var values = 42;

    with (array) {
        test(values, 42);
    }

    array[Symbol.unscopables].values = false;

    with (array) {
        test(values, Array.prototype.values);
    }
}());

(function () {
    var map  = new Map();
    var values = 42;

    with (map) {
        test(values, Map.prototype.values);
    }

    map[Symbol.unscopables] = {
        values: true
    };

    with (map) {
        test(values, 42);
    }
}());

(function () {
    var set  = new Set();
    var values = 42;

    with (set) {
        test(values, Set.prototype.values);
    }

    set[Symbol.unscopables] = {
        values: true
    };

    with (set) {
        test(values, 42);
    }
}());
