function or(a, b) {
    return a | b;
}

let res = 1n;
for (var i = 0; i < 1e5; ++i) {
    res |= or(1n, 2n);
}
