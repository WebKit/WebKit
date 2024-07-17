let m = new Map();

function testSet(m, k, v) {
    m.set(k, v);
}
noInline(testSet);
noDFG(testSet);
noFTL(testSet);

function testGet(m, k) {
    return m.get(k);
}
noInline(testGet);
noDFG(testGet);
noFTL(testGet);

let count = 1e2;
for (let i = 0; i < count; ++i) {
    let s = i.toString();
    testSet(m, s, s);
}

let res;
for (let i = 0; i < count; ++i) {
    let s = i.toString();
    for (let j = 0; j < 1e4; j++) {
        res = testGet(m, s);
    }
}

