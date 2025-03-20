
let arraySize = 20000;

function randArray(min, max) {
    let a = [];
    for (let i = 0; i < arraySize; ++i) {
        a.push(min + Math.floor(Math.random() * (max - min)));
    }
    return a;
}

let comparatorCalls = 0;

function comparator(a, b) {
    ++comparatorCalls;
    return (a % 1337) - (b % 1337);
}

for (let iteration = 0; iteration < 30; ++iteration) {
    let arr = randArray(0, 1048576);
    let tmp = arr.sort();
    // Partially sorted with HUGE runs
    let sorted = tmp.sort(comparator);
    for (let i = 0; i < sorted.length - 1; ++i) {
        if (comparator(sorted[i], sorted[i+1]) > 0) {
            throw new Error("bad");
        }
    }
}

print(comparatorCalls);

