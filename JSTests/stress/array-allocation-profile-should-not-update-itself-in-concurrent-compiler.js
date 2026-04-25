//@ runDefault("--jitPolicyScale=0", "--useArrayAllocationProfiling=0")
function foo() {
    for (let i = 0; i < 30; i++) {
        const ar = [];
        for (let j = 0; j <= 1500; j++) {
            ar[j] = null;
        }
    }
}

foo();
