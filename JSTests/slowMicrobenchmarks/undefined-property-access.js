//@ skip if $architecture == "x86"

var someGlobal;

// This is a simple speed test. It should go fast.

function foo() {
    var myObject = {};
    for (var i = 0; i < 10000000; ++i) {
        someGlobal = myObject.undefinedProperty;
    }
    return someGlobal;
}
result = foo();
if (result != undefined)
    throw new Error("Bad result: " + result);

// This test checks that a cached property lookup miss doesn't continue to fire when the property suddenly appears on the object.

function bar() {
    var myObject = {};
    for (var i = 0; i < 100000000; ++i) {
        someGlobal = myObject.someProperty;
        if (i == 50000000)
            myObject.someProperty = 1;
    }
    return someGlobal;
}
var result = bar();
if (result != 1)
    throw new Error("Bad result: " + result);
someGlobal = undefined;

// This test checks that a cached property lookup miss doesn't continue to fire when the property suddenly appears on the object's prototype.

function baz() {
    var myPrototype = {}
    var myObject = {};
    myObject.__proto__ = myPrototype;
    for (var i = 0; i < 100000000; ++i) {
        someGlobal = myObject.someProperty;
        if (i == 50000000)
            myPrototype.someProperty = 2;
    }
    return someGlobal;
}
var result = baz();
if (result != 2)
    throw new Error("Bad result: " + result);

