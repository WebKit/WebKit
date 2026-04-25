function sameValue(a, b) {
    if (a !== b)
        throw new Error(`Expected ${b} but got ${a}`);
}

function test(array, searchElement, expected) {
    for (let i = 0; i < testLoopCount; i++) {
        sameValue(array.includes(searchElement), expected);
    }
}

test([{}, 3.4, 3.5, 4.5, 5.4, 6.2, 6.2, 4.5], 4.5, true);
test([{}, 3.4, 3.5, 4.5, 5.4, 6.2, 6.2, 4.5], 5.9, false);
