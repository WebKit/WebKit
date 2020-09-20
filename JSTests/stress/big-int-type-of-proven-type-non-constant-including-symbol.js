function assert(a) {
    if (!a)
        throw new Error("Bad assertion");
}

function typeOf(n) {
    var value = "string";
    var dispatcher = n % 3;
    if (dispatcher === 0)
        value = 1n;
    else if (dispatcher === 1)
        value = "string";
    else
        value = Symbol("symbol");
    return typeof value;
}
noInline(typeOf);

for (let i = 0; i < 1e6; i++) {
    switch (i % 3) {
    case 0:
        assert(typeOf(i) === "bigint");
        break;
    case 1:
        assert(typeOf(i) === "string");
        break;
    case 2:
        assert(typeOf(i) === "symbol");
        break;
    }
}
