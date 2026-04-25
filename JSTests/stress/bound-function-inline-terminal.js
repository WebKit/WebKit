//@ runDefault("--jitPolicyScale=0", "--watchdog=5000", "--watchdog-exception-ok")
class C {
    static foo() {
        function bar(a0) {
            try {
                a0(C);
            } catch {
            }
            return a0(a0);
        }
        bar(bar.bind());
    }
}

try {
    C.foo();
} catch {
}
