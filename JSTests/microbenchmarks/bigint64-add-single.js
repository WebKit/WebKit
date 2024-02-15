function add(a, b) {
    return a + b;
}

let res = 0n;
for (var i = 0; i < 1e5; ++i) {
    res += add(1n, 2n);
}
