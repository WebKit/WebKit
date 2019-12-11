function assert(b) {
    if (!b)
        throw new Error("Bad assertion.");
}
function foo(m) {
    return [...m];
}
noInline(foo);

let map = new Map;
map.set(20, 30);
map.set(40, 50);

let called = 0;
let customIterator = {
    [Symbol.iterator]: function() {
        called++;
        let count = 0;
        return {
            next() {
                called++;
                count++;
                if (count === 1)
                    return {done: false, value: [20, 30]};
                if (count === 2)
                    return {done: false, value: [40, 50]};
                return {done: true};
            }
        };
    }
};
for (let i = 0; i < 10000; i++) {
    for (let o of [customIterator, map]) {
        let [[a, b], [c, d]] = foo(o);
        assert(a === 20);
        assert(b === 30);
        assert(c === 40);
        assert(d === 50);
    }
    assert(called === 4);
    called = 0;
}

function bar(m) {
    return [...m, ...m];
}
noInline(bar);
for (let i = 0; i < 10000; i++) {
    for (let o of [customIterator, map]) {
        let [[a, b], [c, d], [e, f], [g, h]] = bar(o);
        assert(a === 20);
        assert(b === 30);
        assert(c === 40);
        assert(d === 50);
        assert(e === 20);
        assert(f === 30);
        assert(g === 40);
        assert(h === 50);
    }
    assert(called === 8);
    called = 0;
}
