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
    arr.push(i + 0.1);
}
delete arr[5];
const result = arr.toReversed();
sameArray(result, [9.1, 8.1, 7.1, 6.1, undefined, 4.1, 3.1, 2.1, 1.1, 0.1]);
