function pow(a, b) {
    return a ** b;
}

let res = 1n;
for (var i = 0; i < 1e5; ++i) {
    res = pow(2n, 2n) ** 2n;
}
