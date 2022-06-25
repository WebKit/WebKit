function test(n) {
    return n === new Object(n);
}
noInline(test);

function assert(condition) {
    if (!condition)
        throw new Error("assertion failed");
}

for (let i = 0; i < 1e5; i++) {
    assert(!test(null));
    assert(!test(undefined));
    assert(!test(1));
    assert(!test(""));
    assert(!test(Symbol.iterator));
    assert(test({}));
    assert(test([]));
}
