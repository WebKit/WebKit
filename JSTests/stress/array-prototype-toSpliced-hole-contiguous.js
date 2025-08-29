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

const newValue1 = { i: 777 };
const newValue2 = { i: 888 };
const result = arr.toSpliced(2, 3, newValue1, newValue2);
sameArray(result, [arr[0], arr[1], newValue1, newValue2, undefined, arr[6], arr[7], arr[8], arr[9]]);
