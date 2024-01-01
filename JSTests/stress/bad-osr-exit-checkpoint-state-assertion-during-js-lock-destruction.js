//@ runDefault("--maximumInliningRecursion=10", "--maximumInliningDepth=10", "--maximumFunctionForConstructInlineCandidateBytecodeCostForDFG=1000", "--maximumFunctionForConstructInlineCandidateBytecodeCostForFTL=1000", "--maximumFunctionForClosureCallInlineCandidateBytecodeCostForDFG=1000", "--maximumFunctionForClosureCallInlineCandidateBytecodeCostForFTL=1000", "--maximumFunctionForCallInlineCandidateBytecodeCostForDFG=1000", "--maximumFunctionForCallInlineCandidateBytecodeCostForFTL=1000", "--useConcurrentJIT=0")

let depth;
const limit = 5;
function foo() {
    if (depth >= limit) {
        OSRExit();
        dropAllLocks();
        return 0;
    }
    ++depth;
    foo.apply(null, a);
}
let a = {
    get length() {
        return 0;
    }
};

for (let i = 0; i < 1e5; ++i) {
    depth = 0;
    foo.apply(null, a);
}
