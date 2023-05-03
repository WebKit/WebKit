//@ runDefault("--jitPolicyScale=0", "--useLICMFuzzing=1")
function foo() {
    let s = Symbol();
    for (let i = 0; i < 2; i++) {
        let z0 = [];
        let z1 = [];
        '' + s + '';
        let z2 = [];
        let z3 = [];
        let z4 = [];
        let z5 = [];
        let z6 = [];
        let z7 = [];
        let z8 = [];
        let z9 = [];
        let z10 = [];
        let z11 = [];
    }
};

for (let i = 0; i < 10000; i++) {
    try { foo(); } catch { }
}
