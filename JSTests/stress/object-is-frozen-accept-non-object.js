var primitives = [
    "string",
    42,
    null,
    undefined,
    Symbol("symbol"),
    true,
    false
];

for (var primitive of primitives) {
    var ret = Object.isFrozen(primitive);
    if (ret !== true)
        throw new Error("bad value: " + String(ret));
}
