function test(set) {
    return [...set];
}
noInline(test);

const set = new Set([1, 2, 3, 4, 5, 6, 7, 8, 9, 10]);

for (let i = 0; i < 1e6; i++) {
    test(set);
}
