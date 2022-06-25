function assert(b) {
    if (!b)
        throw new Error("Bad assertion")
}
noInline(assert);

function test(f, count = 1000) {
    for (let i = 0; i < count; i++)
        f();
}

function foo(a = function() { return c; }, ...[b = function() { return a; }, ...c]) {
    assert(b()() === c);
    assert(a() === c);
}


test(function() {
    foo(undefined, undefined, {});
});

function bar(a, ...{c}) {
    return c;    
}
test(function() {
    assert(bar(10, 20, 30) === undefined);
});

function baz(...[{b}, {b: c}, ...d]) {
    return [b, c, d];
}
test(function() {
    let o = {};

    let result = baz({b: 20}, {b: 30}, 40, o);
    assert(result.length === 3);
    assert(result[0] === 20);
    assert(result[1] === 30);
    assert(result[2].length === 2);
    assert(result[2][0] === 40);
    assert(result[2][1] === o);
});

function jaz(...[...[...c]]) {
    return c;
}
test(function() {
    let result = jaz(10, 20);
    assert(result.length === 2);
    assert(result[0] === 10);
    assert(result[1] === 20);
});

let raz = (a, ...[b, ...c]) => {
    return [b, ...c];
};
test(function() {
    let result = raz(10, 20, 30, 40);
    assert(result.length === 3);
    assert(result[0] === 20);
    assert(result[1] === 30);
    assert(result[2] === 40);
});

Array.prototype.c = 500;
test(function() {
    assert(bar(10, 20, 30) === 500);
});

raz = (a, ...[b = function() { return c; }, ...c]) => {
    return b();
};
test(function() {
    let result = raz(undefined, undefined, 20, 30);
    assert(result.length === 2);
    assert(result[0] === 20);
    assert(result[1] === 30);
});

raz = (a, ...[b = function() { return c; }, d = b(), ...c]) => { };
test(function() {
    let threw = false;
    try {
        raz(undefined, undefined, undefined, undefined);
    } catch(e) {
        threw = e instanceof ReferenceError; }
    assert(threw);
});
