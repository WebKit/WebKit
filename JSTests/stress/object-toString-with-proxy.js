let foo = {};
let properties = [];
let p = new Proxy(foo, { get:(target, property) => {
    properties.push(property.toString());
    if (property === Symbol.toStringTag)
        return "bad things";
    return target[property];
}});

for (i = 0; i < 5; i++) {
    if (p != "[object bad things]")
        throw new Error("bad toString result.");

    if (properties[0] !== "Symbol(Symbol.toPrimitive)" || properties[1] !== "valueOf" || properties[2] !== "toString" || properties[3] !== "Symbol(Symbol.toStringTag)")
        throw new Error("bad property accesses.");

    properties = [];
}

p = createProxy(foo);

for (i = 0; i < 5; i++) {
    let str = "bad things" + i;
    foo[Symbol.toStringTag] = str;
    if (p != "[object " + str + "]")
        throw new Error("bad toString result.");
}
