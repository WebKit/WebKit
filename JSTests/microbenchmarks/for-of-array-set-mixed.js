function test(iterable) {
    let sum = 0;
    for (const v of iterable)
        sum += v;
    return sum;
}
noInline(test);

const arr = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10];
const set = new Set([1, 2, 3, 4, 5, 6, 7, 8, 9, 10]);

for (let i = 0; i < 1e5; ++i) {
    test(arr);
    test(set);
}
