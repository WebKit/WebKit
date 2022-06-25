//@ runDefault("--useConcurrentJIT=false")

function assert(a) {
    if (!a)
        throw new Error("Bad assertion");
}

function foo(a, b) {
    return a === b;
}
noInline(foo);

for (let i = 0; i < 100000; i++) {
    assert(!foo(2n, 3n));
    assert(foo(3n, 3n));
}

assert(!foo(3, 3n));
assert(!foo(0.33, 3n));
assert(!foo("3", 3n));
assert(!foo(Symbol("3"), 3n));
assert(!foo(true, 3n));
assert(!foo(false, 3n));
assert(!foo(NaN, 3n));
assert(!foo(null, 3n));
assert(!foo(undefined, 3n));
assert(!foo(+Infinity, 3n));
assert(!foo(-Infinity, 3n));

function bar() {
    return 3n;
}
noInline(bar);

for (let i = 0; i < 100000; i++)
    assert(bar() === bar());

