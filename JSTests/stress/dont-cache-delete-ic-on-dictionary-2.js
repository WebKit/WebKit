function assert(b) {
    if (!b)
        throw new Error;
}

function makeDictionary() {
    let o = {};
    for (let i = 0; i < 1000; ++i)
        o["f" + i] = 42;
    return o;
}

function foo(o) {
    delete o.x;
}

let d = makeDictionary();
for (let i = 0; i < 150; ++i) {
    foo(d);
}

d.x = 42;
foo(d);

assert(!d.x);
