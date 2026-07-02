// Regression tests for the Iterator.prototype.toArray / forEach / etc. fast path
// in forEachInIteratorProtocol, and Array.from(map.keys() / .values()) fast path
// in tryCreateArrayFromMapIterator. Requirements:
//   1. Seed iteration from the iterator's stored storage (not the map's current),
//      so transit() can shift entry indices across rehashes/clears.
//   2. Leave the iterator drained when iteration completes.
//   3. The fast path and the user-visible callback share the iterator's cursor:
//      a reentrant iter.next() inside the callback must advance the iterator,
//      consuming the next element so the helper does not see it again.
//      Closing the iterator inside the callback (iter.return-equivalent) must
//      stop the helper.

function shouldEq(actual, expected, msg) {
    const a = JSON.stringify(actual);
    const e = JSON.stringify(expected);
    if (a !== e)
        throw new Error("FAIL " + msg + ": got " + a + ", expected " + e);
}

// --- Iterator.prototype.toArray on Map/Set iterators ---

// Partial-consume returns the remaining elements.
{
    const s = new Set([1, 2, 3]);
    const it = s.values();
    it.next();
    shouldEq(it.toArray(), [2, 3], "set toArray after partial consume");
}

// Iterator must be drained after toArray.
{
    const s = new Set([1, 2, 3]);
    const it = s.values();
    it.toArray();
    shouldEq(it.next(), { value: undefined, done: true }, "set: next() after toArray must be done");
}
{
    const s = new Set([1, 2, 3]);
    const it = s.values();
    it.next();
    it.toArray();
    shouldEq(it.next(), { value: undefined, done: true }, "set: next() after partial+toArray must be done");
}
{
    const m = new Map([[1, "a"], [2, "b"], [3, "c"]]);
    const it = m.entries();
    it.toArray();
    shouldEq(it.next(), { value: undefined, done: true }, "map: next() after toArray must be done");
}

// Rehash with deletions during partial consume.
{
    const s = new Set();
    s.add("a"); s.add("b"); s.add("c"); s.add("d");
    const it = s.values();
    it.next(); it.next();
    s.delete("a"); s.delete("b");
    s.add("e");
    shouldEq(it.toArray(), ["c", "d", "e"], "set toArray after rehash with deletions");
}

// Drained iterator across clear().
{
    const s = new Set();
    for (let i = 0; i < 1000; i++) s.add(i);
    const it = s.values();
    while (!it.next().done) {}
    s.clear();
    shouldEq(it.toArray(), [], "set toArray after drain+clear");
}

// --- Reentrancy: callback advances the iterator, helper picks up after it ---

// Set: callback calls iter.next() once per element, "stealing" alternate entries.
// Helper should see the rest. Total elements distributed between helper & callback
// must equal the iterator contents in spec order.
{
    const s = new Set([10, 20, 30, 40, 50]);
    const it = s.values();
    const helperSaw = [];
    const userTook = [];
    it.forEach(v => {
        helperSaw.push(v);
        const n = it.next();
        if (!n.done)
            userTook.push(n.value);
    });
    shouldEq(helperSaw, [10, 30, 50], "set forEach: helper sees alternate elements when callback steals one each step");
    shouldEq(userTook, [20, 40], "set forEach: callback's iter.next() takes interleaved elements");
    shouldEq(it.next(), { value: undefined, done: true }, "set: drained after forEach");
}

// Map: same shape.
{
    const m = new Map([[1, "a"], [2, "b"], [3, "c"], [4, "d"], [5, "e"]]);
    const it = m.values();
    const helperSaw = [];
    const userTook = [];
    it.forEach(v => {
        helperSaw.push(v);
        const n = it.next();
        if (!n.done)
            userTook.push(n.value);
    });
    shouldEq(helperSaw, ["a", "c", "e"], "map forEach: helper sees alternate elements");
    shouldEq(userTook, ["b", "d"], "map forEach: callback gets the rest");
    shouldEq(it.next(), { value: undefined, done: true }, "map: drained after forEach");
}

// Callback drains the iterator entirely via repeated iter.next() — helper must
// then see no more elements.
{
    const s = new Set([1, 2, 3, 4, 5]);
    const it = s.values();
    const helperSaw = [];
    const userTook = [];
    it.forEach(v => {
        helperSaw.push(v);
        if (v === 1) {
            while (true) {
                const n = it.next();
                if (n.done) break;
                userTook.push(n.value);
            }
        }
    });
    shouldEq(helperSaw, [1], "set forEach: helper stops after callback drains rest");
    shouldEq(userTook, [2, 3, 4, 5], "set forEach: callback drained the rest");
    shouldEq(it.next(), { value: undefined, done: true }, "set: drained");
}

// --- Array.from(map.keys()) / Array.from(map.values()) ---

// Partial-consume returns remaining elements.
{
    const m = new Map([[1, "a"], [2, "b"], [3, "c"]]);
    const it = m.keys();
    it.next();
    shouldEq(Array.from(it), [2, 3], "Array.from(map.keys()) after partial consume");
}

// Iterator drained after Array.from.
{
    const m = new Map([[1, "a"], [2, "b"], [3, "c"]]);
    const it = m.keys();
    Array.from(it);
    shouldEq(it.next(), { value: undefined, done: true }, "map: next() after Array.from must be done");
}
{
    const m = new Map([[1, "a"], [2, "b"], [3, "c"]]);
    const it = m.values();
    it.next();
    Array.from(it);
    shouldEq(it.next(), { value: undefined, done: true }, "map: next() after partial+Array.from must be done");
}

// Fresh iterator on a map with deletions — fast path skips empties.
{
    const m = new Map([[1, "a"], [2, "b"], [3, "c"], [4, "d"]]);
    m.delete(2);
    m.delete(3);
    shouldEq(Array.from(m.keys()), [1, 4], "Array.from(map.keys()) fresh, with deletions");
}

// Partial consume across a rehash with deletions.
{
    const m = new Map();
    m.set("a", 1); m.set("b", 2); m.set("c", 3); m.set("d", 4);
    const it = m.keys();
    it.next(); it.next();
    m.delete("a"); m.delete("b");
    m.set("e", 5);
    shouldEq(Array.from(it), ["c", "d", "e"], "Array.from(map.keys()) after rehash with deletions");
}

// Repeat under JIT tiers.
for (let i = 0; i < 1000; i++) {
    const m = new Map([[1, "a"], [2, "b"], [3, "c"]]);
    const it = m.keys();
    it.next();
    shouldEq(Array.from(it), [2, 3], "Array.from(map.keys()) loop");
    shouldEq(it.next(), { value: undefined, done: true }, "iterator drained loop");
}
