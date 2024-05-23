
let m = new Map();

function testSet(m, k, v) {
    m.set(k, v);
}
noDFG(testSet);
noFTL(testSet);
noInline(testSet);

for (let i = 0; i < 1e6; ++i) {
    testSet(m, i, i);
}
