function mul(a, b) {
    return a * b;
}

let res = 1n;
for (var i = 0; i < 1e5; ++i) {
    res *= mul(1n, 1n);
}
