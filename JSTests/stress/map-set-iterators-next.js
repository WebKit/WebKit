//@ runDefault("--useConcurrentJIT=0", "--jitPolicyScale=0.001")
for (let i = 0; i < 100; i++) {
    const m = new Map();
    let itr = m.entries();
    m.set('1', '11');
    const result = itr.next();
    if (result.value === undefined || result.value[0] !== '1' || result.value[1] !== '11')
        throw new Error("bad!");
}

for (let i = 0; i < 100; i++) {
    const m = new Set();
    let itr = m.entries();
    m.add('1');
    const result = itr.next();
    if (result.value === undefined || result.value[0] !== '1' || result.value[1] !== '1')
        throw new Error("bad!");
}
