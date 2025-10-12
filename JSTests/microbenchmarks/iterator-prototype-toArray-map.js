function test(mapIter) {
    return Iterator.prototype.toArray.call(mapIter);
}

const map = new Map();
for (let i = 0; i < 12; i++) {
    map.set(i, i * 2);
}

let r;
for (let i = 0; i < testLoopCount; i++) {
    r = test(map.values());
}
