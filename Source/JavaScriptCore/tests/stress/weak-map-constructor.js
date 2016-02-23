// WeakMap constructor behaviors.

if (typeof WeakMap !== 'function')
    throw "Error: bad value" + typeof WeakMap;

function testCallTypeError(item) {
    var error = null;
    try {
        var map = WeakMap(item);
    } catch (e) {
        error = e;
    }
    if (!error)
        throw "Error: error not thrown";
    if (String(error) !== "TypeError: calling WeakMap constructor without new is invalid")
        throw "Error: bad error " + String(error);
}
var obj1 = {};
var obj2 = [];
var obj3 = new Date();
var obj4 = new Error();

var pass = [
    null,
    undefined,
    [],
    new Set(),
    new Map(),
    "",

    [
        [obj1, 1],
        [obj2, 2],
        [obj3, 3],
    ],

    [
        [obj1, 1],
        [obj1, 2],
        [obj1, 3],
    ],

    [
        { 0: obj2, 1: 'O' },
        { 0: obj3, 1: 'K' },
        { 0: obj4, 1: 'K' },
    ],

    new Map([
        [obj1, 1],
        [obj2, 2],
        [obj3, 3],
    ]),

    new Map([
        [obj1, 1],
        [obj1, 2],
        [obj1, 3],
    ]),

    new Map([
        { 0: obj2, 1: 'O' },
        { 0: obj3, 1: 'K' },
        { 0: obj4, 1: 'K' },
    ]),
];

for (var value of pass) {
    var map = new WeakMap(value);
    testCallTypeError(value);
}

function testTypeError(item, message) {
    var error = null;
    try {
        var map = new WeakMap(item);
    } catch (e) {
        error = e;
    }
    if (!error)
        throw "Error: error not thrown";
    if (!message)
        message = "TypeError: Type error";
    if (String(error) !== message)
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

var nonObjectKeys = [
    [
        [0, 1],
        [1, 2],
        [1, 3],
    ],

    [
        [1, 1],
        [1, 2],
        [1, 3],
    ],

    [
        { 0: 'C', 1: 'O' },
        { 0: 'C', 1: 'K' },
        { 0: 'V', 1: 'K' },
    ],
];

for (var item of nonObjectKeys) {
    testTypeError(item, 'TypeError: Attempted to set a non-object key in a WeakMap');
    testCallTypeError(item);
}
