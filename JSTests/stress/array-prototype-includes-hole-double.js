function sameValue(a, b) {
    if (a !== b)
        throw new Error(`Expected ${b} but got ${a}`);
}

const array = [1.4, 2.4, , 4.4, 5.5];

for (let i = 0; i < testLoopCount; i++) {
    sameValue(array.includes(undefined), true);
}
