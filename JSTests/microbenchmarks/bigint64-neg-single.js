function neg(a) {
    return -a;
}

let res = 1n;
for (var i = 0; i < 1e5; ++i) {
    res = -(neg(res));
}
