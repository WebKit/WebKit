//@ skip if not $jitTests
//@ runDefault("--jitPolicyScale=0.1", "--watchdog-exception-ok", "--watchdog=1000", "--useConcurrentGC=0", "--useLLInt=0", "--validateExceptionChecks=1", "--alwaysUseShadowChicken=1", "--useConcurrentJIT=0", "--useOSREntryToFTL=0", "--maximumInliningRecursion=1", "--thresholdForOptimizeAfterLongWarmUp=2690", "--forceGCSlowPaths=1", "--useZombieMode=1", "--useArityFixupInlining=0", "--useLoopUnrolling=0", "--useB3TailDup=0", "--airRandomizeRegs=1", "--useOMGInlining=0")

// 32 bit ARM turns off JIT by default, causing an error because useLLInt and useJIT can't both be false

if ($vm.useJIT()) {
    for (let i = 0; i - 4194304; i++) {
        for (let j = 0n; j < 2n ** 31n;)
            break;
    }
}
