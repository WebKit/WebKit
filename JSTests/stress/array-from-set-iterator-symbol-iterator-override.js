function shouldBe(a, b) {
    if (a !== b)
        throw new Error(`Expected ${b} but got ${a}`);
}

function shouldBeArray(a, b) {
    shouldBe(a.length, b.length);
    for (let i = 0; i < a.length; i++)
        shouldBe(a[i], b[i]);
}

// Array.from performs GetMethod(items, @@iterator) and iterates whatever that method
// returns, so an own or inherited Symbol.iterator override must be respected.

// Own Symbol.iterator on the set iterator.
{
    const iterator = new Set([1, 3]).keys();
    Object.defineProperty(iterator, Symbol.iterator, { value: () => [42][Symbol.iterator]() });
    shouldBeArray(Array.from(iterator), [42]);
}
{
    const iterator = new Set([1, 3]).values();
    Object.defineProperty(iterator, Symbol.iterator, { value: () => [42][Symbol.iterator]() });
    shouldBeArray(Array.from(iterator), [42]);
}

// Own Symbol.iterator on %SetIteratorPrototype%.
{
    const setIteratorPrototype = Object.getPrototypeOf(new Set().values());
    Object.defineProperty(setIteratorPrototype, Symbol.iterator, { value: () => [43][Symbol.iterator](), configurable: true });
    shouldBeArray(Array.from(new Set([1, 3]).values()), [43]);
    delete setIteratorPrototype[Symbol.iterator];
    shouldBeArray(Array.from(new Set([1, 3]).values()), [1, 3]);
}

// Replaced %IteratorPrototype%[Symbol.iterator].
{
    const iteratorPrototype = Object.getPrototypeOf(Object.getPrototypeOf(new Set().values()));
    const original = iteratorPrototype[Symbol.iterator];
    iteratorPrototype[Symbol.iterator] = () => [44][Symbol.iterator]();
    shouldBeArray(Array.from(new Set([1, 3]).values()), [44]);
    iteratorPrototype[Symbol.iterator] = original;
    shouldBeArray(Array.from(new Set([1, 3]).values()), [1, 3]);
}

// Unrelated own property does not disable the fast path result.
{
    const iterator = new Set([1, 3]).values();
    iterator.foo = 42;
    shouldBeArray(Array.from(iterator), [1, 3]);
}
