function assert(b) {
    if (!b)
        throw new Error("Bad assertion!");
}
noInline(assert);

let value = false;

function baz(x) {
    if (typeof x !== "number") {
        value = true;
    }
    return x;
}
noInline(baz);

function bar(...args) {
    return args;
}

let didEffects = false;
function effects() { didEffects = true; }
noInline(effects);

function foo(a) {
    let args = [1];
    let theArgs = [...args, a, ...args];
    baz(a);
    if (value) {
        effects();
    }
    let r = bar.apply(null, theArgs);
    return r;
}
noInline(foo);

for (let i = 0; i < 100000; i++) {
    foo(i);
    assert(!didEffects);
}
let o = {};
let [a, b, c] = foo(o);
assert(a === 1);
assert(b === o);
assert(c === 1);
assert(didEffects);
