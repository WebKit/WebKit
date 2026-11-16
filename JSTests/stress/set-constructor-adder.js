// Set constructor with adder change.

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

var originalAdder = Set.prototype.add;
var counter = 0;

Set.prototype.add = function (value) {
    counter++;
    return originalAdder.call(this, value);
};

var values = [0, 1, 2, 3, 4, 5, 4, 3, 2, 1, 0];
var set = new Set(values);
if (set.size !== 6)
    throw "Error: bad set size " + set.size;
if (counter !== values.length)
    throw "Error: bad counter " + counter;

Set.prototype.add = function () {
    throw new Error("adder called");
};

var set = new Set();
var set = new Set([]);

shouldThrow(() => {
    new Set([0]);
}, "Error: adder called");

Set.prototype.add = Symbol();
shouldThrow(() => {
    new Set([0]);
}, "TypeError: 'add' property of a Set should be callable.");
