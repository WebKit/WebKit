function bigInt(a, b) {
    let c = a >> b;
    return (a >> c) - b;
}
noInline(bigInt);

for (let i = 0; i < 100000; i++) {
    bigInt(0b1111n, 2n);
}

let out;
for (let i = 0; i < 100000; i++) {
    out = bigInt(0xffffffffffffffffffn, 64n);
}

