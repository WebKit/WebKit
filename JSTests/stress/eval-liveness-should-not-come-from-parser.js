//@ runDefault("--validateGraphAtEachPhase=1", "--validateFTLOSRExitLiveness=1", "--useConcurrentJIT=0", "--jitPolicyScale=0", "--maximumFunctionForCallInlineCandidateBytecodeCostForDFG=300", "--maximumFunctionForCallInlineCandidateBytecodeCostForFTL=300")

function foo(a0, a1) {
    if (a0) {
        for (let i=0; i<100; i++) {
            foo(a1);
            eval(...[]);
        }
    }
}

foo(1, 1);
