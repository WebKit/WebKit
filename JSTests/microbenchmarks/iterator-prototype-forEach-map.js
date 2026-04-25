function test(mapIter) {
    let sum = 0;
    Iterator.prototype.forEach.call(mapIter, function(value) {
        sum += value;
    });
    return sum;
}

const map = new Map();
for (let i = 0; i < 12; i++) {
    map.set(i, i * 2);
}

let r;
for (let i = 0; i < testLoopCount; i++) {
    r = test(map.values());
}
