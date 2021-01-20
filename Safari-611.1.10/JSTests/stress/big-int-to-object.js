function assert(a) {
    if (!a)
        throw new Error("Bad assertion");
}

function foo() {
    assert(typeof this === "object")
}

foo.apply(BigInt(1));

