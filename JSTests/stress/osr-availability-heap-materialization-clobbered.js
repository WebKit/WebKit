//@ requireOptions("--validateFTLOSRExitLiveness=1")

// testLoopCount wasn't on our software update branch when this test was added.
globalThis.testLoopCount ??= 10000;

function opt() {
    for (let i = 0; i < 5; i++) {
        function arg() {
            return arguments;
        }
        const arg1 = arg();
        const arg2 = arg.apply();
        try {
            (3881)(arg2);
        } catch(e) {
            arg1.toString()
        }
    }
}
for (let i = 0; i < testLoopCount; i++) {
    opt();
}
gc();
