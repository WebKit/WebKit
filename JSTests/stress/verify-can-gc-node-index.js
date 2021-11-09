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
f();
