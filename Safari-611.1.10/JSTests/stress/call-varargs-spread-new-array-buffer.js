function assert(b, m = "") {
    if (!b)
        throw new Error("Bad assert: " + m);
}
noInline(assert);

function bar(...args) {
    return args;
}
noInline(bar);

function foo() {
    let args = [1, 2, 3];
    let x = bar(...args, 42, ...args);
    return x;
}
noInline(foo);

for (let i = 0; i < 10000; i++) {
    let r = foo();
    assert(r.length === 7);
    assert(r[0] === 1, JSON.stringify(r));
    assert(r[1] === 2, JSON.stringify(r));
    assert(r[2] === 3, JSON.stringify(r));
    assert(r[3] === 42, JSON.stringify(r));
    assert(r[4] === 1, JSON.stringify(r));
    assert(r[5] === 2, JSON.stringify(r));
    assert(r[6] === 3, JSON.stringify(r));
}
