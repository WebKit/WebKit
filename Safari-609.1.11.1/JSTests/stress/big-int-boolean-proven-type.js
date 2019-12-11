//@ runBigIntEnabled

function assert(a) {
    if (!a)
        throw new Error("Bad assertion");
}

function bool(n) {
    var value = "string";
    if (n & 0x1)
        value = 0n;
    return !!value;
}
noInline(bool);

for (let i = 0; i < 1e6; i++) {
    if (i & 0x1)
        assert(bool(i) === false);
    else
        assert(bool(i) === true);
}
