//@ skip if !$isFTLPlatform
//@ runDefault("--useTestingHelpers=1", "--useDollarVM=1", "--useConcurrentJIT=0", "--useLoopUnrolling=0")

// When IRO proves a Branch condition is one-sided, the downstream pipeline
// folds the branch away. Each case asserts the runtime return and that the
// Branch is gone (folded) or kept (unprovable), counted from the graph dump.

load("./resources/iro-test-helpers.js", "caller relative");

function check(label, fn, args, expectReturn, expectFolded) {
    for (let i = 0; i < 200000; ++i) fn(...args);
    const got = fn(...args);
    if (got !== expectReturn)
        throw new Error(label + ": runtime return mismatch — expected "
            + expectReturn + " got " + got);
    const iro = makeIROHelper(fn);
    const branches = iro.opCount("Branch");
    if (expectFolded) {
        if (branches !== 0)
            throw new Error(label + ": expected the branch to be folded away "
                + "(Branch=0), got Branch=" + branches);
    } else {
        if (branches === 0)
            throw new Error(label + ": expected the branch to remain (the "
                + "condition is unprovable), but it was folded away");
    }
}

function fnLessTrue(i) {
    const x = i & 7;
    let r = 0;
    if (x < 8) r = 1;
    return $vm.probe("r", r);
}
noInline(fnLessTrue);
check("CompareLess TRUE", fnLessTrue, [3], 1, true);

function fnLessFalse(i) {
    const x = i & 7;
    let r = 0;
    if (x < 0) r = 1;
    return $vm.probe("r", r);
}
noInline(fnLessFalse);
check("CompareLess FALSE", fnLessFalse, [3], 0, true);

function fnLessEqTrue(i) {
    const x = i & 7;
    let r = 0;
    if (x <= 7) r = 1;
    return $vm.probe("r", r);
}
noInline(fnLessEqTrue);
check("CompareLessEq TRUE", fnLessEqTrue, [3], 1, true);

function fnLessEqFalse(i) {
    const x = i & 7;
    let r = 0;
    if (x <= -1) r = 1;
    return $vm.probe("r", r);
}
noInline(fnLessEqFalse);
check("CompareLessEq FALSE", fnLessEqFalse, [3], 0, true);

function fnGreaterTrue(i) {
    const x = i & 7;
    let r = 0;
    if (x > -1) r = 1;
    return $vm.probe("r", r);
}
noInline(fnGreaterTrue);
check("CompareGreater TRUE", fnGreaterTrue, [3], 1, true);

function fnGreaterFalse(i) {
    const x = i & 7;
    let r = 0;
    if (x > 7) r = 1;
    return $vm.probe("r", r);
}
noInline(fnGreaterFalse);
check("CompareGreater FALSE", fnGreaterFalse, [3], 0, true);

function fnGreaterEqTrue(i) {
    const x = i & 7;
    let r = 0;
    if (x >= 0) r = 1;
    return $vm.probe("r", r);
}
noInline(fnGreaterEqTrue);
check("CompareGreaterEq TRUE", fnGreaterEqTrue, [3], 1, true);

function fnGreaterEqFalse(i) {
    const x = i & 7;
    let r = 0;
    if (x >= 8) r = 1;
    return $vm.probe("r", r);
}
noInline(fnGreaterEqFalse);
check("CompareGreaterEq FALSE", fnGreaterEqFalse, [3], 0, true);

function fnStrictEqFalse(i) {
    const x = i & 7;
    let r = 0;
    if (x === 99) r = 1;
    return $vm.probe("r", r);
}
noInline(fnStrictEqFalse);
check("CompareStrictEq FALSE", fnStrictEqFalse, [3], 0, true);

function fnLogicalNotTrue(i) {
    const x = i & 7;
    const c = !(x < 0);
    let r = 0;
    if (c) r = 1;
    return $vm.probe("r", r);
}
noInline(fnLogicalNotTrue);
check("LogicalNot TRUE", fnLogicalNotTrue, [3], 1, true);

function fnLogicalNotFalse(i) {
    const x = i & 7;
    const c = !(x < 8);
    let r = 0;
    if (c) r = 1;
    return $vm.probe("r", r);
}
noInline(fnLogicalNotFalse);
check("LogicalNot FALSE", fnLogicalNotFalse, [3], 0, true);

function fnUnprovable(a, b) {
    let r = 0;
    if (a < b) r = 1;
    return $vm.probe("r", r);
}
noInline(fnUnprovable);
check("CompareLess unprovable", fnUnprovable, [1, 2], 1, false);

print("PASS");
