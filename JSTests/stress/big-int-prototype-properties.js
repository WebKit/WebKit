function assert(a) {
    if (!a)
        throw new Error("Bad assertion");
}

(() => {
    let p = Object.getOwnPropertyDescriptor(BigInt.prototype, "toString");

    assert(p.enumerable === false);
    assert(p.configurable === true);
    assert(p.writable === true);
})();

(() => {
    let p = Object.getOwnPropertyDescriptor(BigInt.prototype, "toLocaleString");

    assert(p.enumerable === false);
    assert(p.configurable === true);
    assert(p.writable === true);
})();

(() => {
    let p = Object.getOwnPropertyDescriptor(BigInt.prototype, "valueOf");

    assert(p.enumerable === false);
    assert(p.configurable === true);
    assert(p.writable === true);
})();

(() => {
    let p = Object.getOwnPropertyDescriptor(BigInt.prototype, Symbol.toStringTag);

    assert(p.enumerable === false);
    assert(p.configurable === true);
    assert(p.writable === false);
})();

