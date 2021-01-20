// WeakMap constructor with adder change.

var originalAdder = WeakMap.prototype.set;
var counter = 0;

WeakMap.prototype.set = function (key, value) {
    counter++;
    return originalAdder.call(this, key, value);
};

var obj0 = {};
var obj1 = {};
var obj2 = [];
var obj3 = new Date();
var obj4 = new Error();
var obj5 = JSON;

var values = [
    [ obj0, 0 ],
    [ obj1, 1 ],
    [ obj2, 2 ],
    [ obj3, 3 ],
    [ obj4, 4 ],
    [ obj5, 5 ],
    [ obj4, 4 ],
    [ obj3, 3 ],
    [ obj2, 2 ],
    [ obj1, 1 ],
    [ obj0, 0 ],
];
var map = new WeakMap(values);
if (counter !== values.length)
    throw "Error: bad counter " + counter;

WeakMap.prototype.set = function () {
    throw new Error("adder called");
};

var map = new WeakMap();
var map = new WeakMap([]);
var error = null;
try {
    var map = new WeakMap([ [0, 0] ]);
} catch (e) {
    error = e;
}
if (!error)
    throw "Error: error not thrown";
if (String(error) !== "Error: adder called")
    throw "Error: bad error " + String(error);

