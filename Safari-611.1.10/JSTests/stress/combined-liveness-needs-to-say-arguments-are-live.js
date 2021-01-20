//@ runDefault("--jitPolicyScale=0", "--validateFTLOSRExitLiveness=1", "--useConcurrentJIT=0")

// This should not crash in liveness validation.

function baz() { }
noInline(baz);

function foo() {
    let i, j;
    let a0 = [0, 1];
    let a1 = [];
    for (i = 0; i < a0.length; i++) {
        a1.push();
        for (j = 0; j < 6; j++) {
        }
        for (j = 0; j < 4; j++) {
            baz();
        }
    }
    throw new Error();
}
try {
    new foo();
} catch { }
