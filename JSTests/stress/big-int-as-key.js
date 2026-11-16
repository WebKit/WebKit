function assert(a) {
    if (!a)
        throw new Error("Bad assertion");
}

let o = {};
let n = BigInt(0);

o[n] = "foo";
assert(o[n] === "foo");
assert(o["0"] === "foo");

