function assert(b) {
    if (!b)
        throw new Error;
}

let p = {};
let o = {};
o.__proto__ = p;

for (let i = 0; i < 1000; ++i) {
    p["p" + i] = i;
}
p.foo = 42;
delete p.p0;
delete p.p1;
delete p.p2;
delete p.p3;
delete p.p4;
delete p.p5;

for (let i = 0; i < 10; ++i) {
    p.foo = i;
    assert(o.foo === i);
}
