function assert(a) {
    if (!a)
        throw new Error("Bad assertion");
}

let p = Object.getOwnPropertyDescriptor(BigInt, "prototype");

assert(p.writable === false);
assert(p.enumerable === false);
assert(p.configurable === false);

