// Regression test for the JIT-inlined MapIteratorNext / SetIteratorNext path
// and the C++ helper transitAndNext.
//
// JSOrderedHashTable's DataTable is sized to its load-factor cap (DataCapacity)
// rather than the bucket count, so a Map/Set populated to exactly DataCapacity
// has no spare zero-initialized slot inside the DataTable past the last entry.
// The iterator (both the JIT-inlined fast path and the C++ helper) terminates
// when it loads an empty JSValue from the slot one past the last alive entry;
// without a trailing IterationSentinel slot, that load would run off the end
// of the storage and read whatever happens to be in adjacent memory, which can
// either yield bogus extra entries, loop forever on a stray deleted-marker, or
// crash. ASan reliably catches the OOB read; in normal release builds the bug
// surfaces only when adjacent memory happens to hold a non-zero value, so this
// test exercises the path but is not a deterministic detector on its own.
//
// The sizes packed below are exactly DataCapacity at the small-capacity
// regime, where DataCapacity = capacity / 2.  If the dataCapacity formula in
// JSOrderedHashTableHelper ever changes, regenerate these values.
//   capacity 8   -> DataCapacity 4
//   capacity 32  -> DataCapacity 16
//   capacity 128 -> DataCapacity 64

function assert(b, msg) {
    if (!b)
        throw new Error("FAIL: " + msg);
}
noInline(assert);

const fullSizes = [4, 16, 64];

function buildMap(n) {
    const m = new Map();
    for (let i = 0; i < n; ++i)
        m.set(i, i * 2);
    return m;
}

function buildSet(n) {
    const s = new Set();
    for (let i = 0; i < n; ++i)
        s.add(i);
    return s;
}

function drainMap(m) {
    const it = m[Symbol.iterator]();
    let count = 0;
    let sumK = 0;
    let sumV = 0;
    while (true) {
        const { value, done } = it.next();
        if (done) {
            assert(value === undefined, "done iterator value");
            break;
        }
        sumK += value[0];
        sumV += value[1];
        ++count;
    }
    // One additional next() after done must keep returning done with no error.
    const after = it.next();
    assert(after.done === true, "post-done iterator");
    return [count, sumK, sumV];
}
noInline(drainMap);

function drainSet(s) {
    const it = s[Symbol.iterator]();
    let count = 0;
    let sum = 0;
    while (true) {
        const { value, done } = it.next();
        if (done) {
            assert(value === undefined, "done iterator value");
            break;
        }
        sum += value;
        ++count;
    }
    const after = it.next();
    assert(after.done === true, "post-done iterator");
    return [count, sum];
}
noInline(drainSet);

for (const n of fullSizes) {
    const map = buildMap(n);
    const set = buildSet(n);

    const expectedSumK = (n * (n - 1)) / 2;
    const expectedSumV = expectedSumK * 2;
    const expectedSetSum = expectedSumK;

    for (let i = 0; i < testLoopCount; ++i) {
        const [c, sk, sv] = drainMap(map);
        assert(c === n, "map count " + n);
        assert(sk === expectedSumK, "map sumK " + n);
        assert(sv === expectedSumV, "map sumV " + n);

        const [sc, ss] = drainSet(set);
        assert(sc === n, "set count " + n);
        assert(ss === expectedSetSum, "set sum " + n);
    }
}
