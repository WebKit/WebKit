function assert(b) {
    if (!b)
        throw new Error("Bad assertion")
}
noInline(assert);

function foo(o1, o2) {
    let a = o1.f1;
    let b = o2.f2;
    return a + o1.f1 + b;
}
noInline(foo);

let objs = [];
const count = 80;
for (let i = 0; i < count; i++) {
    let o = {};
    for (let j = 0; j < i; ++j) {
        o[j + "J"] = j;
    }
    o.f1 = 20;
    o.f2 = 40;
    objs.push(o);
}

for (let i = 0; i < 1000; i++) {
    let o1 = objs[i % objs.length];
    let o2 = objs[(i + 1) % objs.length];
    assert(foo(o1, o2) === 80);
}

let o = objs[count - 1];
let numCalls = 0;
Object.defineProperty(o, "f1", {
    get() { ++numCalls; return 25; }
});

assert(foo(o, objs[count - 2]) === 90);
assert(numCalls === 2);
