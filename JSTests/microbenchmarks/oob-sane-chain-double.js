function assert(b) {
    if (!b)
        throw new Error;
}

function foo(a, x) {
    return a[x] * 2.5;
}
noInline(foo);

let a = [1.5, 2.5, , 3.5];

for (let i = 0; i < 10000000; ++i) {
    assert(foo(a, 0) === 2.5 * 1.5);
    assert(isNaN(foo(a, 5)));
    assert(isNaN(foo(a, 2)));
}
