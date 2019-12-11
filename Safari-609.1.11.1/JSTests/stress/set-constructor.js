// Set constructor behaviors.

if (typeof Set !== 'function')
    throw "Error: bad value" + typeof Set;

function testCallTypeError(item) {
    var error = null;
    try {
        var set = Set(item);
    } catch (e) {
        error = e;
    }
    if (!error)
        throw "Error: error not thrown";
    if (String(error) !== "TypeError: calling Set constructor without new is invalid")
        throw "Error: bad error " + String(error);
}

var pass = [
    [ null, 0 ],
    [ undefined, 0 ],
    [ "Cocoa", 4 ],
    [ [0, 1, 2, 3, 4], 5 ],
    [ [0, 0, 0, 1, 0], 2 ],
    [ ["A", "B", "A"], 2 ],
    [ new String("cocoa"), 3 ],
    [ new String("Cocoa"), 4 ],
    [ new Set([0,1,2,3,4]), 5],
    [ new Set([1,1,1,1]), 1],
    [ new Map([[1, 2],[1, 2]]), 1],
    [ new Map([[1, 2],[2, 2]]), 2],
];

for (var pair of pass) {
    var set = new Set(pair[0]);
    if (set.size !== pair[1])
        throw "Error: bad set size " + set.size;
    testCallTypeError(pair[0]);
}

function testTypeError(item) {
    var error = null;
    try {
        var set = new Set(item);
    } catch (e) {
        error = e;
    }
    if (!error)
        throw "Error: error not thrown";
    if (String(error) !== "TypeError: Type error")
        throw "Error: bad error " + String(error);
}

var nonIterable = [
    42,
    Symbol("Cappuccino"),
    true,
    false,
    {},
    new Date(),
    new Error(),
    Object(Symbol("Matcha")),
    (function () { }),
];

for (var item of nonIterable) {
    testTypeError(item);
    testCallTypeError(item);
}
