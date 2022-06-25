function assert(b) {
    if (!b)
        throw new Error;
}

let called = false;
function defineSetter() {
    Array.prototype.__defineSetter__(0, function (x) {
        assert(x === 42);
        called = true;
    });
}

class DerivedArray extends Array {
    constructor(...args) {
        super()
    }
}

function iterate(a) {
    for (let i = 0; i < a.length; i++) { }
}

let arr = [[[1, 2, 3, 4, 5], [ 2], 5], [[1, 2, 3], [ -4]]];
let d = new DerivedArray();
d[1] = 20;
d[2] = 40;
arr.push([d, [2]  -9]);

function doSlice(a) {
    let r = a.slice();
    defineSetter();
    return r;
}

for (let i = 0; i < 10000; i++) {
    for (let [a, b, ...c] of arr) {
        let s = doSlice(a);
        iterate(s);
        delete s[0];
        called = false;
        s[0] = 42;
        if (a === d) {
            assert(called);
            called = false;
        }
    }
}
