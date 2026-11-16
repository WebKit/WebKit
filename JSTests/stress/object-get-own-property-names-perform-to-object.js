var primitives = [
    ["string", ['0', '1', '2', '3', '4', '5', 'length']],
    [42, []],
    [Symbol("symbol"), []],
    [true, []],
    [false, []]
];

function compare(ax, bx) {
    if (ax.length !== bx.length)
        return false;
    for (var i = 0, iz = ax.length; i < iz; ++i) {
        if (ax[i] !== bx[i])
            return false;
    }
    return true;
}

for (var [primitive, expected] of primitives) {
    var ret = Object.getOwnPropertyNames(primitive);
    if (!compare(ret, expected))
        throw new Error("bad value for " + String(primitive) + ": " + String(ret));
}

[
    [ null, "TypeError: null is not an object (evaluating 'Object.getOwnPropertyNames(value)')" ],
    [ undefined, "TypeError: undefined is not an object (evaluating 'Object.getOwnPropertyNames(value)')" ]
].forEach(function ([value, message]) {
    var error = null;
    try {
        Object.getOwnPropertyNames(value);
    } catch (e) {
        error = e;
    }
    if (!error)
        throw new Error("error not thrown");
    if (String(error) !== message)
        throw new Error("bad error: " + String(error));
});
