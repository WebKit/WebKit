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
delete arr[5];
const result = arr.with(2, 555);
sameArray(result, [0, 1, 555, 3, 4, undefined, 6, 7, 8, 9]);
