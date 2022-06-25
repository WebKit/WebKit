function assert(a) {
    if (!a)
        throw new Error("Bad assertion");
}

let proto = Object.getPrototypeOf(BigInt.prototype);

assert(proto === Object.prototype);

