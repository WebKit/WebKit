//@ runDefault("--returnEarlyFromInfiniteLoopsForFuzzing=1", "--earlyReturnFromInfiniteLoopsLimit=1000", "--jitPolicyScale=0", "--maximumFunctionForCallInlineCandidateBytecodeCostForDFG=1000", "--maximumFunctionForCallInlineCandidateBytecodeCostForFTL=1000", "--useConcurrentJIT=0", "--useFTLJIT=0")
const a = [null, 0, 0, 0, 0, 0, 0];

function foo() {
  for (let i=0; i<10; i++) {}
}

for (let i=0; i<10000; i++) {
  a.sort(foo);
}
