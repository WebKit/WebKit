let m = new Map();

function testSet(m, k, v) {
    m.set(k, v);
}

let count = 1e4;
for (let i = 0; i < count; ++i) {
    testSet(m, i, i);
}
