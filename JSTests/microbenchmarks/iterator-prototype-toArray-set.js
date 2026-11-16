function test(setIter) {
    return Iterator.prototype.toArray.call(setIter);
}

const set = new Set();
for (let i = 0; i < 12; i++) {
    set.add(i);
}

let r;
for (let i = 0; i < 1e4; i++) {
    r = test(set.values());
}
