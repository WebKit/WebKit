//@ runBigIntEnabled

function assert(a) {
    if (!a)
        throw new Error("Bad assertion");
}

function typeOf(n) {
    var value = "string";
    if (n & 0x1)
        value = 1n;
    return typeof value;
}
noInline(typeOf);

for (let i = 0; i < 1e6; i++) {
    if (i & 0x1)
        assert(typeOf(i) === "bigint");
    else
        assert(typeOf(i) === "string");
}
