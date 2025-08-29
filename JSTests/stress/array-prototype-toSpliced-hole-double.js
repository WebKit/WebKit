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
const result = arr.toSpliced(2, 3, 777.7, 888.8);
sameArray(result, [0.1, 1.1, 777.7, 888.8, undefined, 6.1, 7.1, 8.1, 9.1]);
