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
    const filtered = iter.filter(x => x % 2 === 0);
    assert(filtered.next(), { done: false, value: 2 });
    assert(filtered.next(), { done: false, value: 4 });
    assert(filtered.next(), { done: true });
    const refiltered = iter.filter(x => x % 2 === 0);
    assert(refiltered.next(), { done: true });
}

{
    const iter = gen();
    const filtered = iter.filter(x => x % 2 === 0);
    assert(filtered.next(), { done: false, value: 2 });
    assert(filtered.return(), { done: true });
    assert(filtered.next(), { done: true });
    const refiltered = iter.filter(x => x % 2 === 0);
    assert(refiltered.next(), { done: true });
}
