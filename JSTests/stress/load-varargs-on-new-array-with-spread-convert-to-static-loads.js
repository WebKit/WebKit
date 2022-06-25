function assert(b) {
    if (!b)
        throw new Error("Bad!");
}
noInline(assert);

function baz(...args) {
    return args;
}
function bar(a, ...args) {
    return baz(...args, 42, ...args);
}
function foo(a, b, c, d) {
    return bar(a, b, c, d);
}
noInline(foo);

for (let i = 0; i < 10000; i++) {
    let r = foo(i, i+1, i+2, i+3);
    assert(r.length === 7);
    assert(r[0] === i+1);
    assert(r[1] === i+2);
    assert(r[2] === i+3);
    assert(r[3] === 42);
    assert(r[4] === i+1);
    assert(r[5] === i+2);
    assert(r[6] === i+3);
}
