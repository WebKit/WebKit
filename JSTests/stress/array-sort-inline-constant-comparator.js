// Verifies the DFG ArraySortIntrinsic's comparator body-inlining path when
// the comparator is a constant JSFunction (named global / hoisted) rather
// than a closure. The parser detects this via Node::hasConstant() +
// dynamicCastConstant<JSFunction*>() and routes through handleCall with
// CallLinkStatus(CallVariant(function)).

function cmp(a, b) {
    return a - b;
}

function test(arr) {
    arr.sort(cmp);
}

const original = [5, 3, 8, 1, 9, 4, 7, 2, 6, 0, 11, 14, 13, 10, 12, 15];
const expected = original.slice().sort(cmp);

for (let i = 0; i < testLoopCount; i++) {
    const copy = original.slice();
    test(copy);
    for (let j = 0; j < expected.length; j++) {
        if (copy[j] !== expected[j])
            throw new Error("iter " + i + " index " + j
                + ": got " + copy[j] + " expected " + expected[j]);
    }
}
