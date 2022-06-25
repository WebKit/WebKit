//@ runDefault("--useConcurrentJIT=0")

for (let i=0; i < 10000; i++) {
    for (let j=0n; j < 2n**31n;)
        break;
}
