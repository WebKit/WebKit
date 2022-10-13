// Often hits JSCTEST_memoryLimit on ARM with --memory-limited.
//@ skip if $memoryLimited
//@ runDefault("--destroy-vm", "--maximumFunctionForCallInlineCandidateBytecodeCost=500", "--maximumInliningRecursion=5")

function* gen() {
}
let g = gen();
function f() {
    g.next();
    f();
    f();
    f();
    f();
    f();
    f();
    f();
    f();
    f();
    f();
};
try {
    f();
} catch { }
