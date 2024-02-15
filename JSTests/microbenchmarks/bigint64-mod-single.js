function mod(a, b) {
    return a % b;
}

let res = 1n;
for (var i = 0; i < 1e5; ++i) {
    res = mod(1n, 1n) % 1n;
}
