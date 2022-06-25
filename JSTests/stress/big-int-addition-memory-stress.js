function assert(a) {
    if (!a)
        throw new Error("Bad assertion");
}

let a = 0n;
for (let i = 0; i < 1000000; i++) {
    a += 30n;
}

assert(a === 30000000n);

