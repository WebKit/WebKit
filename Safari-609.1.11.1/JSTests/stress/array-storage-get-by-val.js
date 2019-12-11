'use strict';

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var object = { a: 10 };
Object.defineProperties(object, {
    "0": {
        get: function() { return this.a; },
        set: function(x) { this.a = x; },
    },
});

var array = [ 0, 1, 2, 3, 4, 5 ];
ensureArrayStorage(array);

function testOutOfBound()
{
    var results = 0;
    for (var i = 0; i < 1e5; ++i) {
        for (var j = 0; j < 7; ++j) {
            var value = array[j];
            if (value !== undefined)
                results += value;
        }
    }
    return results;
}
noInline(testOutOfBound);

function testInBound()
{
    var results = 0;
    for (var i = 0; i < 1e5; ++i) {
        for (var j = 0; j < 6; ++j)
            results += array[j];
    }
    return results;
}
noInline(testInBound);

var slowPutArray = [ 0, 1, 2, 3, 4, 5 ];
ensureArrayStorage(slowPutArray);
slowPutArray.__proto__ = object;

function testSlowPutOutOfBound()
{
    var results = 0;
    for (var i = 0; i < 1e5; ++i) {
        for (var j = 0; j < 7; ++j) {
            var value = slowPutArray[j];
            if (value !== undefined)
                results += value;
        }
    }
    return results;
}
noInline(testSlowPutOutOfBound);

function testSlowPutInBound()
{
    var results = 0;
    for (var i = 0; i < 1e5; ++i) {
        for (var j = 0; j < 6; ++j)
            results += slowPutArray[j];
    }
    return results;
}
noInline(testSlowPutInBound);

shouldBe(testOutOfBound(), 1500000);
shouldBe(testInBound(), 1500000);
shouldBe(testSlowPutOutOfBound(), 1500000);
shouldBe(testSlowPutInBound(), 1500000);
