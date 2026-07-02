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
    var ret = Object.isExtensible(primitive);
    if (ret !== false)
        throw new Error("bad value: " + String(ret));
}
