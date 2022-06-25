function assert(b) {
    if (!b)
        throw new Error("Bad assertion")
}
noInline(assert);

function bar(...rest) {
    return rest;
}

function foo(a, b, c) {
    return bar(a, b, c);
}
noInline(foo);

for (let i = 0; i < 10000; i++) {
    let result = foo(10, 20, 30);
    assert(result.length === 3);
    assert(result[0] === 10);
    assert(result[1] === 20);
    assert(result[2] === 30);
}

function baz(...rest) {
    return rest;
}
function jaz(a, b, c) {
    return baz.apply(null, Array.prototype.slice.call(arguments));
}
noInline(jaz);

for (let i = 0; i < 50000; i++) {
    let result = jaz(10, 20, 30);
    assert(result.length === 3);
    assert(result[0] === 10);
    assert(result[1] === 20);
    assert(result[2] === 30);
}
