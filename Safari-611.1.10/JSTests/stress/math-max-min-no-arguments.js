function assert(b) {
    if (!b)
        throw new Error("Bad!")
}

function min() {
    return Math.min();
}

function max() {
    return Math.max();
}

function test() {
    for (let i = 0; i < 10000; i++) {
        assert(min() === Infinity);
        assert(max() === -Infinity);
    }
}
test();
