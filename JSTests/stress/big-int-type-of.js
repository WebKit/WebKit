//@ runBigIntEnabled

function assert(a) {
    if (!a)
        throw new Error("Bad assertion");
}

assert(typeof 0n === "bigint");
assert(typeof 1n !== "object");

