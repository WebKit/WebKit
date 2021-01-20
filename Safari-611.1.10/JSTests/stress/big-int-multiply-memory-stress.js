function assert(a) {
    if (!a)
        throw new Error("Bad assertion");
}

let a = 0n;
let b = 1n;
for (let i = 0; i < 1000000; i++) {
    a = b * 30n;
}

assert(a === 30n);

