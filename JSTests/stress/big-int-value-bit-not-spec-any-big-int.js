function assert(b) {
    if (!b)
        throw new Error;
}
function foo(x) {
    return ~x;
}
noInline(foo);

for (let i = 0; i < 1000000; ++i) {
    let x = 1n;
    assert(foo(x) === (0n - x) - 1n);

    x = 10101010101010101010101010101010101010101010101010n;
    assert(foo(x) === (0n - x) - 1n);
}
