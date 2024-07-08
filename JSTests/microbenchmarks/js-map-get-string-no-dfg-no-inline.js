let m = new Map();

function testSet(m, k, v) {
    m.set(k, v);
}
noDFG(testSet);
noInline(testSet);

function testGet(m, k) {
    m.get(k);
}
noDFG(testGet);
noInline(testGet);

let count = 1e4;
for (let i = 0; i < count; ++i) {
    let s = i.toString();
    testSet(m, s, s);
}

for (let i = 0; i < count; ++i) {
    let s = i.toString();
    for (let j = 0; j < 20; j++) {
        testGet(m, s);
    }
}

