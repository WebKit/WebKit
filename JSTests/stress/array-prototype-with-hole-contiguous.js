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
    arr.push({ i });
}
delete arr[5];

const value = { i: 555 };
const result = arr.with(2, value);
sameArray(result, [arr[0], arr[1], value, arr[3], arr[4], undefined, arr[6], arr[7], arr[8], arr[9]]);
