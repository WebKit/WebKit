function assert(a) {
    if (!a)
        throw new Error("Bad assertion");
}

(() => {
    let p = Object.getOwnPropertyDescriptor(BigInt, "asUintN");

    assert(p.enumerable === false);
    assert(p.configurable === true);
    assert(p.writable === true);
})();

(() => {
    let p = Object.getOwnPropertyDescriptor(BigInt, "asIntN");

    assert(p.enumerable === false);
    assert(p.configurable === true);
    assert(p.writable === true);
})();

