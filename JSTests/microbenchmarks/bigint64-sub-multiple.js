function sub(a, b) {
    return a - b - a - b - b - a - b - b;
}

let res = 0n;
for (var i = 0; i < 1e5; ++i) {
    res -= sub(1n, 2n);
}
