//@ runNoisyTestDefault("--useConcurrentJIT=0", "--thresholdForJITAfterWarmUp=10", "--thresholdForOptimizeAfterWarmUp=100", "--thresholdForFTLOptimizeAfterWarmUp=100", "--verboseOSR=1", "--poisonDeadOSRExitVariables=1")

const N = 2000;
const big = Array.from({ length: N }, (_, i) => i);

function* gen(n) {
    for (let i = 0; i < n; i++)
        yield i;
}

function sum(a) {
    let s = 0;
    for (let i = 0; i < a.length; i++)
        s += a[i];
    return s;
}

function test(it) {
    try {
        throw it;
    } catch (e) {
        let b = [];
        for (let x of e)
            b.push(x);
        return sum(b);
    }
}
noInline(test);

for (let iter = 0; iter < 30; iter++) {
    test(big);
    test(gen(N));
}
