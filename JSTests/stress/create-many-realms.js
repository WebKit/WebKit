 //@ runDefault("--collectContinuously=1", "--collectContinuouslyPeriodMS=20", "--useGenerationalGC=0", "--useStochasticMutatorScheduler=0", "--useDFGJIT=0", "--useFTLJIT=0", "--maxPerThreadStackUsage=1000000")

function foo(count) {
    const x = createGlobalObject();
    if (count === 100)
        return;
    return foo(count + 1);
}
foo(0);
