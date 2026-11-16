function shouldBe(actual, expected, msg) {
    if (actual !== expected)
        throw new Error((msg ? msg + ": " : "") + `expected ${expected} but got ${actual}`);
}

function shouldThrow(func) {
    let threw = false;
    try {
        func();
    } catch {
        threw = true;
    }
    shouldBe(threw, true, "should have thrown");
}

// forEachInIterable can take a fast path for cross-realm JSArrays whose own
// realm's array iterator protocol is pristine. When the callback throws,
// IteratorClose must look up `return` on the iterator that the spec would have
// created, i.e. one inheriting from the *array's own realm's*
// %ArrayIteratorPrototype%, not the caller realm's.
{
    const other = createGlobalObject();
    const otherArray = other.Array.of(1, 2, 3);

    let callerRealmReturnCount = 0;
    const ArrayIteratorPrototype = Object.getPrototypeOf([].values());
    ArrayIteratorPrototype.return = function () {
        callerRealmReturnCount++;
        return { value: undefined, done: true };
    };

    // new WeakSet(otherArray): primitive entries cause the adder callback to
    // throw a TypeError, which triggers IteratorClose on the (materialized)
    // array iterator. Per spec that iterator inherits from the other realm's
    // %ArrayIteratorPrototype%, which has no `return`, so nothing should run.
    shouldThrow(() => new WeakSet(otherArray));
    shouldBe(callerRealmReturnCount, 0, "Array: caller realm %ArrayIteratorPrototype%.return must not be called");

    delete ArrayIteratorPrototype.return;
}

// Also confirm that the fast path is observably equivalent for the non-throwing case.
{
    const other = createGlobalObject();
    const otherArray = other.Array.of(1, 2, 3, 3, 2, 1);

    let callCount = 0;
    const ArrayIteratorPrototype = Object.getPrototypeOf([].values());
    const originalNext = ArrayIteratorPrototype.next;
    ArrayIteratorPrototype.next = function () {
        callCount++;
        return originalNext.call(this);
    };

    const set = new Set(otherArray);
    shouldBe(set.size, 3, "Set size");
    // The other realm's %ArrayIteratorPrototype%.next is unmodified, so the
    // fast path (or even the generic path against the other realm's iterator)
    // must never call the caller realm's overridden next.
    shouldBe(callCount, 0, "Array: caller realm %ArrayIteratorPrototype%.next must not be called");

    ArrayIteratorPrototype.next = originalNext;
}
