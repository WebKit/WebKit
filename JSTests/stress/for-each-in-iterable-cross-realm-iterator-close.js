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

// When forEachInIterable takes the JSSet storage fast path and the callback
// throws, IteratorClose must look up `return` on the iterator that the spec
// would have created, i.e. one inheriting from the *Set's own realm's*
// %SetIteratorPrototype%, not the caller realm's.
{
    const other = createGlobalObject();
    // A Set from the other realm whose iterator protocol is pristine.
    const otherSet = new other.Set([1, 2, 3]);

    let callerRealmReturnCount = 0;
    const SetIteratorPrototype = Object.getPrototypeOf(new Set().values());
    SetIteratorPrototype.return = function () {
        callerRealmReturnCount++;
        return { value: undefined, done: true };
    };

    // new WeakSet(otherSet): primitive entries cause the adder callback to
    // throw a TypeError, which triggers IteratorClose on the (materialized)
    // set iterator. Per spec that iterator inherits from the other realm's
    // %SetIteratorPrototype%, which has no `return`, so nothing should run.
    shouldThrow(() => new WeakSet(otherSet));
    shouldBe(callerRealmReturnCount, 0, "Set: caller realm %SetIteratorPrototype%.return must not be called");

    delete SetIteratorPrototype.return;
}

// Same for the JSMap storage fast path.
{
    const other = createGlobalObject();
    const otherMap = new other.Map();
    otherMap.set("k", "v");

    let callerRealmReturnCount = 0;
    const MapIteratorPrototype = Object.getPrototypeOf(new Map().entries());
    MapIteratorPrototype.return = function () {
        callerRealmReturnCount++;
        return { value: undefined, done: true };
    };

    // new WeakMap(otherMap): the entry key "k" is not an object/symbol, so the
    // adder callback throws a TypeError, which triggers IteratorClose.
    shouldThrow(() => new WeakMap(otherMap));
    shouldBe(callerRealmReturnCount, 0, "Map: caller realm %MapIteratorPrototype%.return must not be called");

    delete MapIteratorPrototype.return;
}
