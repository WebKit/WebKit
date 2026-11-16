//@ runDefault("--useConcurrentJIT=0", "--forceWeakRandomSeed=1", "--jitPolicyScale=0")

for (let i = 0; i < 8; i++) {
    for (let j = 0; j < 8; j++) {
        const t1 = j + 2147483649;
        const t2 = (2147483649 >>> (t1 & i)) - 2147483649;
        const t3 = t2 * t1;
        const t4 = t2 + t3;
        if (i == 3 && j === 0) {
            if (t3 !== -2305843012434919400)
                throw new Error("bad");
        }
    }
}
