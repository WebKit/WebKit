// WeakSet constructor behaviors.

if (typeof WeakSet !== 'function')
    throw new Error("bad value" + typeof WeakSet);

function testCallTypeError(item) {
    var error = null;
    try {
        var set = WeakSet(item);
    } catch (e) {
        error = e;
    }
    if (!error)
        throw new Error("error not thrown");
    if (String(error) !== "TypeError: calling WeakSet constructor without new is invalid")
        throw new Error("bad error " + String(error));
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
        obj1,
        obj2,
        obj3,
    ],

    [
        obj1,
        obj1,
        obj1,
    ],

    [
        obj2,
        obj3,
        obj4,
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

    new Set([
        obj1,
        obj2,
        obj3,
    ]),

    new Set([
        obj1,
        obj1,
        obj1,
    ]),

    new Map([
        { 0: obj2, 1: 'O' },
        { 0: obj3, 1: 'K' },
        { 0: obj4, 1: 'K' },
    ]),

    new Set([
        obj2,
        obj3,
        obj4,
    ])
];

for (var value of pass) {
    var set = new WeakSet(value);
    testCallTypeError(value);
}

function testTypeError(item, message) {
    var error = null;
    try {
        var set = new WeakSet(item);
    } catch (e) {
        error = e;
    }
    if (!error)
        throw new Error("error not thrown");
    if (!message)
        message = "TypeError: Type error";
    if (String(error) !== message)
        throw new Error("bad error " + String(error));
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

var nonObjectKeys = [
    [
        0,
        1,
        1,
    ],

    [
        1,
        1,
        1,
    ],

    [
        'C',
        'C',
        'V',
    ],
];

for (var item of nonObjectKeys) {
    testTypeError(item, 'TypeError: Attempted to add a non-object key to a WeakSet');
    testCallTypeError(item);
}
