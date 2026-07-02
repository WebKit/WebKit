// WeakMap constructor with adder change.

function shouldThrow(func, errorMessage) {
    var errorThrown = false;
    try {
        func();
    } catch (error) {
        errorThrown = true;
        if (String(error) !== errorMessage)
            throw new Error(`Bad error: ${error}`);
    }
    if (!errorThrown)
        throw new Error("Didn't throw!");
}

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

shouldThrow(() => {
    new WeakMap([ [0, 0] ]);
}, "Error: adder called");

WeakMap.prototype.set = null;
shouldThrow(() => {
    new WeakMap([ [0, 0] ]);
}, "TypeError: 'set' property of a WeakMap should be callable.");
