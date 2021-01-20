function test(n) {
    return n === Object(n);
}
noInline(test);

function assert(condition) {
    if (!condition)
        throw new Error("assertion failed");
}

for (i = 0; i < 100000; i++) {
    assert(!test(null));
    assert(!test(undefined));
    assert(!test(1));
    assert(!test(""));
    assert(!test(Symbol.iterator));
    assert(test({}));
}
