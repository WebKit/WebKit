function sameArray(a, b) {
    if (a.length !== b.length)
        throw new Error(`Expected array length ${b.length} but got ${a.length}`);
    for (let i = 0; i < a.length; i++) {
        if (a[i] !== b[i])
            throw new Error(`Expected ${b[i]} but got ${a[i]} (${i})`);
    }
}


const arr = [];
for (let i = 0; i < 10; i++) {
    arr.push(i);
}
let result = arr.with(0, 42);
sameArray(result, [42, 1, 2, 3, 4, 5, 6, 7, 8, 9]);

result = result.with(6, 87);
sameArray(result, [42, 1, 2, 3, 4, 5, 87, 7, 8, 9]);
