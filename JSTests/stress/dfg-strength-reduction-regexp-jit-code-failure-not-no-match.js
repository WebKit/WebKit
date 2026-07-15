// The Yarr JIT frame for this pattern is large enough to fail the JIT stack check on a DFG
// compiler thread (which has a much smaller stack than the mutator), while the mutator keeps
// matching successfully via the Yarr JIT so no bytecode ever gets compiled. DFG strength
// reduction must treat that compiler-thread failure as "give up folding", not as a no-match.
const termCount = 30000;
const testRegExp = new RegExp("^" + "a?".repeat(termCount) + "$");
const execRegExp = new RegExp("^" + "a?".repeat(termCount) + "(b)?$");
const subject = "a";

function foldableTest() {
    return testRegExp.test(subject);
}
noInline(foldableTest);

function foldableExec() {
    return execRegExp.exec(subject);
}
noInline(foldableExec);

for (let i = 0; i < testLoopCount; i++) {
    if (!foldableTest())
        throw new Error("test() folded to false at iteration " + i);
    if (foldableExec() === null)
        throw new Error("exec() folded to null at iteration " + i);
}
