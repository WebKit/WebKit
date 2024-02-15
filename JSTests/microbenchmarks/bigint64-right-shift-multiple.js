function shiftRight(a, b) {
    return a >> b >> a >> b >> b >> a;
}

function test() {
    let res = 0n;
    let cond = i % 3;
    let shift = 0n;
    if (cond == 1)
        shift = -1n;
    else if (cond == 2)
        shift = 1n;
    res = shiftRight(1n, shift) >> 1n;
    return res;
}

for (var i = 0; i < 1e5; ++i) {
   test();
}
