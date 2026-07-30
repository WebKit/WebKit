function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`Expected ${expected} but got ${actual}`);
}

function shouldBeArray(actual, expected) {
    shouldBe(actual.length, expected.length);
    for (let i = 0; i < expected.length; ++i)
        shouldBe(actual[i], expected[i]);
}

// Basic correctness: keys() and values() are the exact same function (aliased in the spec),
// both produce a JSSetIterator with IterationKind::Values.
{
    let s = new Set([1, 2, 3]);
    shouldBeArray(Array.from(s.values()), [1, 2, 3]);
    shouldBeArray(Array.from(s.keys()), [1, 2, 3]);
    shouldBeArray(Array.from(s[Symbol.iterator]()), [1, 2, 3]);
}

// entries() must not take this fast path -- it still yields [v, v] pairs correctly.
{
    let s = new Set(['a', 'b']);
    let entries = Array.from(s.entries());
    shouldBe(entries.length, 2);
    shouldBeArray(entries[0], ['a', 'a']);
    shouldBeArray(entries[1], ['b', 'b']);
}

// Partially-consumed iterator must resume from its real cursor, not restart from the beginning.
{
    let s = new Set([10, 20, 30, 40]);
    let it = s.values();
    it.next();
    it.next();
    shouldBeArray(Array.from(it), [30, 40]);
}

// Empty set iterator.
{
    let s = new Set();
    shouldBeArray(Array.from(s.values()), []);
}

// Already-exhausted iterator should yield an empty array, not throw.
{
    let s = new Set([1]);
    let it = s.values();
    it.next();
    it.next();
    shouldBeArray(Array.from(it), []);
}

// Own "return" property on the iterator instance forces the slow path
// (per getDirectOffset(returnKeyword) inside setIteratorProtocolIsFastAndNonObservable).
{
    let s = new Set([1, 2, 3]);
    let it = s.values();
    let called = false;
    it.return = function () { called = true; return { done: true, value: undefined }; };
    shouldBeArray(Array.from(it), [1, 2, 3]);
    shouldBe(called, false);
}

// Overriding SetIteratorPrototype.next must be observed, i.e. the fast path must disengage.
{
    let s = new Set([1, 2, 3]);
    let proto = Object.getPrototypeOf(s.values());
    let calls = 0;
    let origNext = proto.next;
    proto.next = function () { calls++; return origNext.call(this); };
    try {
        shouldBeArray(Array.from(s.values()), [1, 2, 3]);
        if (calls === 0)
            throw new Error('overridden next() was never called -- fast path incorrectly bypassed observable override');
    } finally {
        proto.next = origNext;
    }
}

// A Set subclass's own .values()/.keys() iterator still fast-paths correctly: the guard checks
// the iterator's own prototype, not the underlying Set's prototype chain.
{
    class MySet extends Set { }
    let s = new MySet([5, 6, 7]);
    shouldBeArray(Array.from(s.values()), [5, 6, 7]);
}

// Mutating the underlying set between manual next() calls and Array.from of the remaining
// iterator must not crash and must reflect the live storage.
{
    let s = new Set([1, 2]);
    let it = s.values();
    it.next();
    s.add(3);
    s.add(4);
    shouldBeArray(Array.from(it), [2, 3, 4]);
}

// Mixed types exercise the contiguous (non-int32/non-double) storage path.
{
    let s = new Set(['x', { a: 1 }, [1, 2], null, undefined]);
    shouldBe(Array.from(s.values()).length, 5);
}

// All-double-representable values exercise the hasDouble() storage path.
{
    let s = new Set([1.5, 2.5, 3.5]);
    shouldBeArray(Array.from(s.values()), [1.5, 2.5, 3.5]);
}
