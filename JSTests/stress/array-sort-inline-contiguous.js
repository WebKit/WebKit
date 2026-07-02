// Verifies the DFG ArraySortIntrinsic's fast path handles ArrayWithContiguous arrays -- arrays
// whose elements include non-Int32 values (doubles, strings, objects).

function sortIt(a, cmp) { return a.sort(cmp); }

// Contiguous (mixed-type) arrays: objects.
{
    const input = [{v:5},{v:3},{v:1},{v:4},{v:2}];
    const expected = [1,2,3,4,5];
    for (let w = 0; w < testLoopCount; w++) {
        const a = input.slice();
        const r = sortIt(a, (x, y) => x.v - y.v);
        for (let i = 0; i < 5; i++)
            if (r[i].v !== expected[i])
                throw new Error("iter " + w + " index " + i + ": got " + r[i].v + " expected " + expected[i]);
    }
}

// Contiguous (strings).
{
    const input = ["dd", "bb", "aa", "cc"];
    const expected = ["aa", "bb", "cc", "dd"];
    for (let w = 0; w < testLoopCount; w++) {
        const a = input.slice();
        const r = sortIt(a, (x, y) => x < y ? -1 : x > y ? 1 : 0);
        for (let i = 0; i < 4; i++)
            if (r[i] !== expected[i])
                throw new Error("iter " + w + " index " + i + ": got " + r[i] + " expected " + expected[i]);
    }
}

// Contiguous (doubles stored via contiguous indexing since allocated from literal with ints then a double).
{
    const input = [3.5, 1.2, 2.8, 0.4];
    const expected = [0.4, 1.2, 2.8, 3.5];
    for (let w = 0; w < testLoopCount; w++) {
        const a = input.slice();
        const r = sortIt(a, (x, y) => x - y);
        for (let i = 0; i < 4; i++)
            if (r[i] !== expected[i])
                throw new Error("iter " + w + " index " + i + ": got " + r[i] + " expected " + expected[i]);
    }
}
