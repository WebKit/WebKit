//@ requireOptions("--jitPolicyScale=0", "--maximumFunctionForCallInlineCandidateBytecodeCostForDFG=1000", "--maximumFunctionForCallInlineCandidateBytecodeCostForFTL=1000", "--validateFTLOSRExitLiveness=1")

for (let i = 0; i < 100000; i++) {
    (function foo() { [0].concat().indexOf(0) })()
}
