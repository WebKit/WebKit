// WeakSet constructor with adder change.

var originalAdder = WeakSet.prototype.add;
var counter = 0;

WeakSet.prototype.add = function (key) {
    counter++;
    return originalAdder.call(this, key);
};

var obj0 = {};
var obj1 = {};
var obj2 = [];
var obj3 = new Date();
var obj4 = new Error();
var obj5 = JSON;

var values = [
    obj0,
    obj1,
    obj2,
    obj3,
    obj4,
    obj5,
    obj4,
    obj3,
    obj2,
    obj1,
    obj0,
];
var set = new WeakSet(values);
if (counter !== values.length)
    throw new Error("bad counter " + counter);

WeakSet.prototype.add = function () {
    throw new Error("adder called");
};

var set = new WeakSet();
var set = new WeakSet([]);
var error = null;
try {
    var set = new WeakSet([ 0 ]);
} catch (e) {
    error = e;
}
if (!error)
    throw new Error("error not thrown");
if (String(error) !== "Error: adder called")
    throw new Error("bad error " + String(error));

