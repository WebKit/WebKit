function sameValue(a, b) {
    if (a !== b)
        throw new Error(`Expected ${b} but got ${a}`);
}

function test(array, searchElement, expected) {
    for (let i = 0; i < testLoopCount; i++) {
        sameValue(array.includes(searchElement), expected);
    }
}

test([1, 2, 3, 4, 5, 6, 6, 4], 5, true);
test([1, 2, 3, 4, 5, 6, 6, 4], 32, false);
