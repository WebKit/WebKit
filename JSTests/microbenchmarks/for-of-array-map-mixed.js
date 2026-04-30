function test(iterable) {
    let sum = 0;
    for (const e of iterable) {
        if (typeof e === "number")
            sum += e;
        else
            sum += e[0] + e[1];
    }
    return sum;
}
noInline(test);

const arr = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10];
const map = new Map([[1, 10], [2, 20], [3, 30], [4, 40], [5, 50]]);

for (let i = 0; i < 1e5; ++i) {
    test(arr);
    test(map);
}
