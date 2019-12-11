function assert(b) {
    if (!b)
        throw new Error("Bad");
}

function foo(a, b, c) {
    let x = a + b + c;
    return [a, b, c, x];
}
noInline(foo);

Object.defineProperty(Object.prototype, "10000", {get() { return 20; }});

for (let i = 0; i < 10000; ++i) {
    let a = 10.5;
    let b = 1.1;
    let c = 1.2;
    let x = a + b + c;
    let result = foo(a, b, c);
    assert(result.length === 4);
    assert(result[0] === a);
    assert(result[1] === b);
    assert(result[2] === c);
    assert(result[3] === x);
}
