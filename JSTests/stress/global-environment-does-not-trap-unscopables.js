function test(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var global = new Function('return this')();
var Cocoa = 'Cocoa';

global[Symbol.unscopables] = {
    Cocoa: true
};

test(Cocoa, "Cocoa");
(function () {
    var Cocoa = 'local'
    with (global) {
        test(Cocoa, "local");
    }
}());
