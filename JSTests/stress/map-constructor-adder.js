// Map constructor with adder change.

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

var originalAdder = Map.prototype.set;
var counter = 0;

Map.prototype.set = function (key, value) {
    counter++;
    return originalAdder.call(this, key, value);
};

var values = [
    [ 0, 0 ],
    [ 1, 1 ],
    [ 2, 2 ],
    [ 3, 3 ],
    [ 4, 4 ],
    [ 5, 5 ],
    [ 4, 4 ],
    [ 3, 3 ],
    [ 2, 2 ],
    [ 1, 1 ],
    [ 0, 0 ],
];
var map = new Map(values);
if (map.size !== 6)
    throw "Error: bad map size " + map.size;
if (counter !== values.length)
    throw "Error: bad counter " + counter;

Map.prototype.set = function () {
    throw new Error("adder called");
};

var map = new Map();
var map = new Map([]);

shouldThrow(() => {
    new Map([ [0, 0] ]);
}, "Error: adder called");

Map.prototype.set = undefined;
shouldThrow(() => {
    new Map([ [0, 0] ]);
}, "TypeError: 'set' property of a Map should be callable.");
