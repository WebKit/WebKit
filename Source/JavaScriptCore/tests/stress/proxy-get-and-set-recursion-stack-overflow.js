function assert(b) {
    if (!b)
        throw new Error('bad assertion');
}

function testStackOverflowGet() {
    let threw = false;
    try {
        let o = {};
        let p = new Proxy(o, {});
        Object.setPrototypeOf(o, p);
        p.anyField;
    } catch(e) {
        threw = true;
        assert(e.toString() === "RangeError: Maximum call stack size exceeded.");
    }
    assert(threw);
}

function testStackOverflowIndexedGet(i) {
    let threw = false;
    try {
        let o = {};
        let p = new Proxy(o, {});
        Object.setPrototypeOf(o, p);
        p[i];
    } catch(e) {
        threw = true;
        assert(e.toString() === "RangeError: Maximum call stack size exceeded.");
    }
    assert(threw);
}

function testStackOverflowSet() {
    let threw = false;
    try {
        let o = {};
        let p = new Proxy(o, {});
        Object.setPrototypeOf(o, p);
        p.anyField = 50;
    } catch(e) {
        threw = true;
        assert(e.toString() === "RangeError: Maximum call stack size exceeded.");
    }
    assert(threw);
}

function testStackOverflowIndexedSet(i) {
    let threw = false;
    try {
        let o = {};
        let p = new Proxy(o, {});
        Object.setPrototypeOf(o, p);
        p[i] = 50;
    } catch(e) {
        threw = true;
        assert(e.toString() === "RangeError: Maximum call stack size exceeded.");
    }
    assert(threw);
}

for (let i = 0; i < 250; i++) {
    testStackOverflowGet();
    testStackOverflowIndexedGet(i);
    testStackOverflowSet();
    testStackOverflowIndexedSet(i);
}
