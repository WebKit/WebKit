//@ requireOptions("--jitPolicyScale=0.01")
for (let x = 0; x < 1; (function () {
    for (let i = 0; i < 999999; i++) {
        if (i >= 300000) quit();
        Atomics.compareExchange(new Uint8Array(2), 1, x, x);
    }
})()
) { }
