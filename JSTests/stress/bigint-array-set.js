//@ runDefault("--jitPolicyScale=0")
for (let i = 1; i < 1e3; i++) {
    const v2 = [1000];
    const v4 = new BigUint64Array(i);
    let thrown = false;
    try {
        v4.set(v2);
    } catch (e6) {
        thrown = true;
    }
    if (!thrown)
        throw new Error("bad");
    Object.defineProperty(v2, 10, { writable: true, value: 10 });
}
