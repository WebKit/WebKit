function assert(b) {
    if (!b)
        throw new Error;
}
noInline(assert);

function foo(...args) {
    return args[0];
}
noInline(foo);

for (let i = 0; i < 10000; i++) {
    // Warm it up on in bound accesses.
    assert(foo(i) === i);
}

Array.prototype[0] = 50;
for (let i = 0; i < 10000; i++)
    assert(foo() === 50);
