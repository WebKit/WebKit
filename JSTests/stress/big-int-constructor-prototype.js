function assert(a) {
    if (!a)
        throw new Error("Bad assertion");
}

let proto = Object.getPrototypeOf(BigInt);
assert(proto === Function.prototype);

