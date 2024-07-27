let m = new Map();

function testSet(m, k, v) {
    m.set(k, v);
}
noInline(testSet);
noDFG(testSet);
noFTL(testSet);

function testGet(m, k) {
    m.get(k);
}
noInline(testGet);
noDFG(testGet);
noFTL(testGet);

let count = 1e2;
for (let i = 0; i < count; ++i) {
    testSet(m, i, i);
}

let sum = 0;
for (let i = 0; i < count; ++i) {
    for (let j = 0; j < 1e4; j++) {
        sum += testGet(m, i);
    }
}