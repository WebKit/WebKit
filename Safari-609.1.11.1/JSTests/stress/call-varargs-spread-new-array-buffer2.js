function assert(b, m = "") {
    if (!b)
        throw new Error("Bad assert: " + m);
}
noInline(assert);

function bar(...args) {
    return args;
}
noInline(bar);

function foo(a, ...args) {
    let x = bar(...args, 42, ...[0, 1, 2, 3, 4], ...args); 
    return x;
}
noInline(foo);

for (let i = 0; i < 10000; i++) {
    let r = foo(i, i+1, i+2, i+3);
    assert(r.length === 12);
    assert(r[0] === i+1, JSON.stringify(r));
    assert(r[1] === i+2, JSON.stringify(r));
    assert(r[2] === i+3, JSON.stringify(r));
    assert(r[3] === 42, JSON.stringify(r));
    assert(r[4] === 0, JSON.stringify(r));
    assert(r[5] === 1, JSON.stringify(r));
    assert(r[6] === 2, JSON.stringify(r));
    assert(r[7] === 3, JSON.stringify(r));
    assert(r[8] === 4, JSON.stringify(r));
    assert(r[9] === i+1, JSON.stringify(r));
    assert(r[10] === i+2, JSON.stringify(r));
    assert(r[11] === i+3, JSON.stringify(r));
}
