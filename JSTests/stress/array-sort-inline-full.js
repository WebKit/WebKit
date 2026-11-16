// Full insertion sort inlining: lengths 0..16, fresh closure comparator.

function sortAsc(a, cmp) { return a.sort(cmp); }

// Exercise lengths 0..16 with a fresh arrow comparator on each call.
function runSize(n) {
    let a = [];
    for (let i = n - 1; i >= 0; i--) a.push(i);
    let r = sortAsc(a, (x, y) => x - y);
    if (r.length !== n) throw new Error("len " + n + " wrong length");
    for (let i = 0; i < n; i++) {
        if (r[i] !== i)
            throw new Error("len " + n + " wrong at " + i + ": " + r[i]);
    }
}

// Warm up the DFG compile of sortAsc with a mix of sizes, then exercise the
// compiled path for a while.
for (let w = 0; w < testLoopCount; w++)
    for (let n = 0; n <= 16; n++)
        runSize(n);

// length > 16 must fall back to the generic sort via the slow-path call.
function bigTest() {
    let a = [];
    for (let i = 49; i >= 0; i--) a.push(i);
    let r = sortAsc(a, (x, y) => x - y);
    for (let i = 0; i < 50; i++)
        if (r[i] !== i) throw new Error("big wrong at " + i);
}
for (let i = 0; i < testLoopCount; i++) bigTest();

// Stability check via a pair-of-keys sort.
function stableTest() {
    let items = [];
    for (let i = 0; i < 12; i++)
        items.push({ k: i % 3, seq: i });
    let r = sortAsc(items, (x, y) => x.k - y.k);
    // Within each key group, seq must be preserved in original order.
    for (let i = 1; i < r.length; i++) {
        if (r[i].k < r[i - 1].k)
            throw new Error("unstable: out of key order at " + i);
        if (r[i].k === r[i - 1].k && r[i].seq < r[i - 1].seq)
            throw new Error("unstable: seq reversed at " + i);
    }
}
for (let i = 0; i < testLoopCount; i++) stableTest();

// Undefined comparator should still behave correctly. This falls out of the fast path
// because we speculate CellUse on the comparator.
let r0 = [3, 1, 2].sort();
if (r0[0] !== 1 || r0[1] !== 2 || r0[2] !== 3)
    throw new Error("default sort wrong: " + r0);

// Non-callable comparator must throw TypeError -- matches spec.
let threw = false;
try {
    sortAsc([1, 2, 3], {});
} catch (e) {
    threw = e instanceof TypeError;
}
if (!threw) throw new Error("non-callable comparator did not throw TypeError");
