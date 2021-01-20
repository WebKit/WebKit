function assert(a) {
    if (!a)
        throw new Error("Bad assertion");
}

let p = Object.getOwnPropertyDescriptor(BigInt, "name");
assert(p.enumerable === false);
assert(p.writable === false);
assert(p.configurable === true);
assert(p.value === "BigInt");

