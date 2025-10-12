function test(setIter, cb) {
    return Iterator.prototype.forEach.call(setIter, cb);
}

const set = new Set();
for (let i = 0; i < 12; i++) {
    set.add(i);
}

let r = [];
for (let i = 0; i < 1e4; i++) {
    test(set.values(), function () {
        r.push(i);
    });
}
