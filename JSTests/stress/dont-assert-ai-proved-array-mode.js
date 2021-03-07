//@ requireOptions("--jitPolicyScale=0", "--slowPathAllocsBetweenGCs=15")

function foo(a0, a1, a2, a3, ...a4) {
    function bar() {
        for (let i=0; i<10; i++) {
            new Uint8Array(arguments);
            arguments[0];
            try {
                0..waffles();
            } catch (e) {
            }
            foo.__proto__ = arguments;
        }
    }
    bar();
}
for (let i=0; i<100; i++) {
    foo();
}
