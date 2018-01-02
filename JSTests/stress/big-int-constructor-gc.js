//@ runBigIntEnabled

function assert(a) {
    if (!a)
        throw new Error("Bad assertion");
}

let arr = [];

for (let i = 0; i < 1000000; i++) {
    arr[i] = BigInt(i.toString());
}

gc();

for (let i = 0; i < 1000000; i++) {
    assert(arr[i].toString() === i.toString());
}

