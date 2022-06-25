var primitives = [
    ["string", String.prototype],
    [42, Number.prototype],
    [Symbol("symbol"), Symbol.prototype],
    [true, Boolean.prototype],
    [false, Boolean.prototype]
];

for (var [primitive, expected] of primitives) {
    var ret = Object.getPrototypeOf(primitive);
    if (ret !== expected)
        throw new Error("bad value for " + String(primitive) + ": " + String(ret));
}

[
    [ null, "TypeError: null is not an object (evaluating 'Object.getPrototypeOf(value)')" ],
    [ undefined, "TypeError: undefined is not an object (evaluating 'Object.getPrototypeOf(value)')" ]
].forEach(function ([value, message]) {
    var error = null;
    try {
        Object.getPrototypeOf(value);
    } catch (e) {
        error = e;
    }
    if (!error)
        throw new Error("error not thrown");
    if (String(error) !== message)
        throw new Error("bad error: " + String(error));
});
