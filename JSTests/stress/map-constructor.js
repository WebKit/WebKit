// Map constructor behaviors.

if (typeof Map !== 'function')
    throw "Error: bad value" + typeof Map;

function testCallTypeError(item) {
    var error = null;
    try {
        var map = Map(item);
    } catch (e) {
        error = e;
    }
    if (!error)
        throw "Error: error not thrown";
    if (String(error) !== "TypeError: calling Map constructor without new is invalid")
        throw "Error: bad error " + String(error);
}

var pass = [
    [ null, 0 ],
    [ undefined, 0 ],
    [ [], 0 ],
    [ new Set(), 0],
    [ new Map(), 0],
    [ "", 0],

    [
        [
            [0, 1],
            [1, 2],
            [1, 3],
        ],
        2
    ],

    [
        [
            [1, 1],
            [1, 2],
            [1, 3],
        ],
        1
    ],

    [
        new Map([
            { 0: 'C', 1: 'O' },
            { 0: 'C', 1: 'K' },
            { 0: 'V', 1: 'K' },
        ]),
        2
    ],

    [
        new Map([
            [0, 1],
            [1, 2],
            [1, 3],
        ]),
        2
    ],

    [
        new Map([
            [1, 1],
            [1, 2],
            [1, 3],
        ]),
        1
    ],

    [
        new Map([
            { 0: 'C', 1: 'O' },
            { 0: 'C', 1: 'K' },
            { 0: 'V', 1: 'K' },
        ]),
        2
    ],
];

for (var pair of pass) {
    var map = new Map(pair[0]);
    if (map.size !== pair[1])
        throw "Error: bad map size " + map.size;
    testCallTypeError(pair[0]);
}

function testTypeError(item) {
    var error = null;
    try {
        var map = new Map(item);
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

var notContainNextItem = [
    "Cocoa",
    [0, 1, 2, 3, 4],
    [0, 0, 0, 1, 0],
    ["A", "B", "A"],
    new String("cocoa"),
    new String("Cocoa"),
    new Set([0,1,2,3,4]),
    new Set([1,1,1,1]),
];

for (var item of notContainNextItem) {
    testTypeError(item);
    testCallTypeError(item);
}
