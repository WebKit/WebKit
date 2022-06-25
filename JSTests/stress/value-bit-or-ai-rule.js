function assert(a, e) {
    if (a !== e)
        throw new Error("Expected: " + e + " bug got: " + a);
}

let predicate = true;
function foo(a) {
    let v = a;
    if (predicate)
        v = 0b1000;

    let c = v | 0b11;
    return c;
}
noInline(foo);

for (let i = 0; i < 10000; i++) {
    assert(foo(0b1000n), 0b1011);
}

