//@ runDefault("--jitPolicyScale=0.1", "--watchdog-exception-ok", "--watchdog=5000", "--useLLInt=0", "--validateExceptionChecks=1", "--useConcurrentJIT=0", "--useConcurrentGC=0", "--forceGCSlowPaths=1", "--useOSREntryToFTL=0")

// Regression test for HeapBigInt comparison in DFG peephole branch fusion.
// CompareLess with HeapBigIntUse was falling through to genericJSValuePeepholeBranch
// (GC-capable) instead of the correct non-peephole path, violating doesGC()=false.
// The for-loop structure triggers peephole fusion of CompareLess + Branch.
// The watchdog causes JSLock re-acquisition which triggers DoesGCCheck validation.

for (let i = 0; i < 2000000; i++) {
    for (let j = 0n; j < 2n ** 2n;)
        break;
}
