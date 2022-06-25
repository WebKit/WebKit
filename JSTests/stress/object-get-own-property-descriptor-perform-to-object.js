var primitives = [
    ["string", 6],
    [42, undefined],
    [Symbol("symbol"), undefined],
    [true, undefined],
    [false, undefined]
];

for (var [primitive, expected] of primitives) {
    var ret = Object.getOwnPropertyDescriptor(primitive, 'length');
    if (expected === undefined) {
        if (ret !== expected)
            throw new Error("bad value for " + String(primitive) + ": " + String(ret));
    } else if (ret.value !== expected)
        throw new Error("bad value for " + String(primitive) + ": " + String(ret));
}

function canary() {
    return {
        called: false,
        toString() {
            this.called = true;
            throw new Error("out");
        }
    };
}

[
    [ null, "TypeError: null is not an object (evaluating 'Object.getOwnPropertyDescriptor(value, property)')" ],
    [ undefined, "TypeError: undefined is not an object (evaluating 'Object.getOwnPropertyDescriptor(value, property)')" ]
].forEach(function ([value, message]) {
    var property = canary();
    var error = null;
    try {
        Object.getOwnPropertyDescriptor(value, property);
    } catch (e) {
        error = e;
    }
    if (!error)
        throw new Error("error not thrown");
    if (String(error) !== message)
        throw new Error("bad error: " + String(error));
});
