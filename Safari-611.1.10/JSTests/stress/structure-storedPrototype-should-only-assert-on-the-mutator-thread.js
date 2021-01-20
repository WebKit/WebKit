//@ skip if ["arm", "mips"].include?($architecture)
//@ slow!
//@ runDefault("--jitPolicyScale=0")

// This test should not crash.

// Increase iterations to 10000 if you want the regression to reproduce more reliably.
// It can manifest in just a few iterations or may take a lot more iterations. We're
// reducing iterations here to shorten the execution time of this test for normal runs,
// with the tradeoff that some runs may not trigger the regression (if present). This is
// so that fixed builds (which is the likely case going forward) won't have to wait too
// long for this test to finish.
const iterations = 500;
for (let i = 0; i < iterations; i++) {
    let code = `
        for (let i = 0; i < 1000; i++) {
            String.prototype.__proto__ = [];
            const w = 'abcdefg'[-2];
        }
    `;
    runString(code);
}
