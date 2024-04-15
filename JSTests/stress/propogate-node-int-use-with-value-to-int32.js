function test(e, n) {
    return e[n >>> 5] |= 128 << 24 - n % 32;
}
noInline(test);

arr = [1.1, 1.1, 1.1, 1.1];
for (let i = 0; i < 1e3; i++) {
    test(arr, 10 << 5);
}
