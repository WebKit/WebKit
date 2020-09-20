function assert(a) {
    if (!a)
        throw new Error("Bad assertion");
}

let a = 0b11n;
for (let i = 0; i < 1000000; i++) {
    a &= 0b01n;
}

assert(a === 0b01n);

