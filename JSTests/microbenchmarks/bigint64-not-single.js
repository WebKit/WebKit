function not(a) {
    return ~a;
}

let res = 1n;
for (var i = 0; i < 1e5; ++i) {
    res = ~(not(res));
}
