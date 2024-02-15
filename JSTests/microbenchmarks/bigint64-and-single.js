function and(a, b) {
    return a & b;
}

let res = 1n;
for (var i = 0; i < 1e5; ++i) {
    res &= and(1n, 2n);
}
