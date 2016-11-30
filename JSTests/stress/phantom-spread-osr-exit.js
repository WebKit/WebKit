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

function foo(a, ...args) {
    let r = bar(...args, baz(a), ...args);
    if (value) {
        effects();
    }
    return r;
}
noInline(foo);

for (let i = 0; i < 100000; i++) {
    foo(i, i+1);
    assert(!didEffects);
}
let o = {};
let [a, b, c] = foo(o, 20);
assert(a === 20);
assert(b === o);
assert(c === 20);
assert(didEffects);
