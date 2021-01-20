// Set constructor with adder change.

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
var error = null;
try {
    var set = new Set([0]);
} catch (e) {
    error = e;
}
if (!error)
    throw "Error: error not thrown";
if (String(error) !== "Error: adder called")
    throw "Error: bad error " + String(error);

