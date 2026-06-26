//@ runDefault("--validateDFGClobberize=1", "--jitPolicyScale=0", "--useConcurrentJIT=0")

for (let i = 0; i < 100; i++) {
    for (let j = 0; j < 100; j++) {
    }
}
