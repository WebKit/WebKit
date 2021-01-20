function assert(a) {
    if (!a)
        throw new Error("Bad assertion");
}

let a = 0n;
let b = 30n;
for (let i = 0; i < 1000000; i++) {
    a = b / 2n;
}

assert(a === 15n);

