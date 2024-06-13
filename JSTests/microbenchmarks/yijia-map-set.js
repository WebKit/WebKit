
let m = new Map();

function testSet(m, k, v) {
    m.set(k, v);
}
function testGet(m, k, v) {
    m.get(k, v);
}

function testForIteration(m) {
    for (let [k, v] of m) { }
}

function testForEach(m) {
    m.forEach((value, key, map) => { });
}

for (let i = 0; i < 1e6; ++i) {
    testSet(m, i, i);
}

for (let i = 0; i < 1e6; ++i) {
    testGet(m, i, i);
    testGet(m, i, i);
    testGet(m, i, i);
}

// for (let i = 0; i < 1e3; ++i) {
// testForIteration(m);
// testForEach(m);
// }
