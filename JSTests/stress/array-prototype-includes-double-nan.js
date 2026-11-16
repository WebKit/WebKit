function sameValue(a, b) {
    if (a !== b)
        throw new Error(`Expected ${b} but got ${a}`);
}

const array = [1.2, 2.4, NaN, 4.4, 5.4];

for (let i = 0; i < testLoopCount; i++) {
    sameValue(array.includes(NaN), true);
}
