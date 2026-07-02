function sameValue(a, b) {
    if (a !== b)
        throw new Error(`Expected ${b} but got ${a}`);
}

function test(array, searchElement, expected) {
    for (let i = 0; i < testLoopCount; i++) {
        sameValue(array.includes(searchElement), expected);
    }
}

const sym = Symbol("foo");
test([1, 2, 3.4, sym, 5, 6, 6, 4], sym, true);
test([1, 2, 3.4, Symbol("bar"), 5, 6, 6, 4], sym, false);
