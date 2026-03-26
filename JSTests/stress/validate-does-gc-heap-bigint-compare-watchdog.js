//@ runDefault("--watchdog=5000", "--watchdog-exception-ok", "--jitPolicyScale=0.1", "--useConcurrentGC=0", "--useLLInt=0", "--useConcurrentJIT=0", "--validateDoesGC=true")

// Regression test for rdar://172191300
// Stale DoesGC state from a DFG CompareLess(HeapBigIntUse) node must not
// persist across JSLock release/acquire when the watchdog terminates execution.
if ($vm.useJIT()) {
    for (let i = 0; i - 4194304; i++) {
        for (let j = 0n; j < 2n ** 31n;)
            break;
    }
}
