function assert(a) {
    if (!a)
        throw new Error("Bad assertion");
}

let p = Object.getOwnPropertyDescriptor(this, "BigInt");
assert(p.writable === true);
assert(p.enumerable === false);
assert(p.configurable === true);

