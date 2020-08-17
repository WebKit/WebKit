function assert(b) {
    if (!b)
        throw new Error;
}

function baz(a, x) {
    return a[x];
}
noInline(baz);

let a = [1.5, 2.5, , 3.5];

for (let i = 0; i < 10000000; ++i) {
    assert(baz(a, 0) === 1.5);
    assert(baz(a, 5) === undefined);
    assert(baz(a, 2) === undefined);
}

