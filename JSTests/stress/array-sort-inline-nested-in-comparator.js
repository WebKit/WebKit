//@ requireOptions("--useConcurrentJIT=0")
//
// Nested sort: the inlined comparator itself calls Array.prototype.sort, so the
// inner handleArraySort's private cross-block tmps must not alias the outer's.

function makeIntArr(n) {
    let a = [];
    for (let i = 0; i < n; i++)
        a.push((n - i) | 0);
    return a;
}

let neighbours = [];
for (let i = 0; i < 4096; i++)
    neighbours.push(makeIntArr(3));
let inner = makeIntArr(3);
for (let i = 0; i < 4096; i++)
    neighbours.push(makeIntArr(3));

function innerCmp(a, b) { return 0; }

function outerCmp(a, b) {
    (a instanceof Object); // forces a checkpoint in outerCmp.
    inner.sort(innerCmp);
    return false;
}

function test(arr) { return arr.sort(outerCmp); }
noInline(test);

for (let i = 0; i < testLoopCount; i++)
    test(makeIntArr(5));

for (let i = 0; i < neighbours.length; i++) {
    if (neighbours[i].length !== 3)
        throw new Error("neighbour " + i + " corrupted: length = " + neighbours[i].length);
}
gc();

let result = test(makeIntArr(5));
if (result.length !== 5)
    throw new Error("outer result corrupted: " + JSON.stringify(result));
