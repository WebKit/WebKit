//@ runDefault("--jitPolicyScale=0", "--validateGraphAtEachPhase=1", "--useConcurrentJIT=0", "--validateDFGClobberize=1")

for (let i = 0; i < 100; i++) {
    for (let j = 0; j < 100; j++) {
    }
}
