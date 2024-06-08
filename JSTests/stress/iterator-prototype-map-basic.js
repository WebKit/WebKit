//@ requireOptions("--useIteratorHelpers=1")

function assert(a, b) {
    a = JSON.stringify(a);
    b = JSON.stringify(b);
    if (a !== b)
        throw new Error("Expected: " + b + " but got: " + a);
}

function *gen() {
    yield 1;
    yield 2;
    yield 3;
    yield 4;
}

{
    const iter = gen();
    const mapped = iter.map(x => x * 2);
    assert(mapped.next(), { done: false, value: 2 });
    assert(mapped.next(), { done: false, value: 4 });
    assert(mapped.next(), { done: false, value: 6 });
    assert(mapped.next(), { done: false, value: 8 });
    assert(mapped.next(), { done: true });
    const remapped = iter.map(x => x * 2);
    assert(remapped.next(), { done: true });
}

{
    const iter = gen();
    const mapped = iter.map(x => x * 2);
    assert(mapped.next(), { done: false, value: 2 });
    assert(mapped.return(), { done: true });
    assert(mapped.next(), { done: true });
    const remapped = iter.map(x => x * 2); 
    assert(remapped.next(), { done: true });
}
