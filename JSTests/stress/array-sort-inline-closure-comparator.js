// Verifies the DFG ArraySortIntrinsic's comparator body-inlining path for a
// NewArrowFunction / NewFunction closure comparator allocated fresh on every
// sort call. This is the `.sort((a,b) => a - b)` idiom that every real-world
// sort caller uses; it's the path the parser-level inliner must detect via
// Node::isFunctionAllocation().

function test(arr) {
    arr.sort((a, b) => a - b);
}

const original = [5, 3, 8, 1, 9, 4, 7, 2, 6, 0, 11, 14, 13, 10, 12, 15];
const expected = original.slice().sort((a, b) => a - b);

for (let i = 0; i < testLoopCount; i++) {
    const copy = original.slice();
    test(copy);
    for (let j = 0; j < expected.length; j++) {
        if (copy[j] !== expected[j])
            throw new Error("iter " + i + " index " + j
                + ": got " + copy[j] + " expected " + expected[j]);
    }
}

// Descending comparator -- separate closure shape, also re-allocated per call.
function descendingTest(arr) {
    arr.sort((a, b) => b - a);
}

const expectedDesc = original.slice().sort((a, b) => b - a);
for (let i = 0; i < testLoopCount; i++) {
    const copy = original.slice();
    descendingTest(copy);
    for (let j = 0; j < expectedDesc.length; j++) {
        if (copy[j] !== expectedDesc[j])
            throw new Error("desc iter " + i + " index " + j
                + ": got " + copy[j] + " expected " + expectedDesc[j]);
    }
}
