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
const result = arr.toReversed();
sameArray(result, [9, 8, 7, 6, 5, 4, 3, 2, 1, 0]);
