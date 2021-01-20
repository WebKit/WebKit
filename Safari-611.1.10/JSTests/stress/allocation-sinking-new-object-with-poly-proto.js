function foo() {
    class A {
        constructor() {
        }
    };
    return A;
}
let A = foo();
let B = foo();

function makePolyProto(o) {
    return o.x;
}
noInline(makePolyProto);

for (let i = 0; i < 1000; ++i) {
    makePolyProto(i % 2 ? new A : new B);
}

function bar(b) {
    let o = new A;
    if (b) {
        if (isFinalTier())
            OSRExit();
        return o;
    }
}
noInline(bar);

function baz(b) {
    let o = new A;
    if (b)
        return o;
}
noInline(baz);

for (let i = 0; i < 100000; ++i) {
    let b = i % 10 === 0;
    let r = bar(b);
    if (b) {
        if (r.__proto__ !== A.prototype)
            throw new Error("Bad!");
    }
}

for (let i = 0; i < 100000; ++i) {
    let b = i % 10 === 0;
    let r = baz(b);
    if (b) {
        if (r.__proto__ !== A.prototype)
            throw new Error("Bad!");
    }
}
