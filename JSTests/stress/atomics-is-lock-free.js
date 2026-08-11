
//@ requireOptions("--useConcurrentJIT=0", "--jitPolicyScale=0.001")
let res;
function f2() {
    res = 1;
}
function f1() {
    Int16Array.valueOf = f2;
    this.Atomics.isLockFree(Int16Array);
}

for (let i = 0; i < 10; i++) {
    res = 0;
    f1();
    if (!res)
        throw new Error("bad!");
}
