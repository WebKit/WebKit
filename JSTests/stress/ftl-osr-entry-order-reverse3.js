//@ runDefault("--jitPolicyScale=0")

function foo() {
    function bar(a0, a1, a2) {
        let s = 'a'.replace('', a1);
        for (let i=0; i<100; i++) {
            a0[s] = {};
        }
    }
    bar(0.1, bar);
}


for (let i=0; i<5; i++) {
    runString(`${foo.toString()}; for (let i=0; i<100; i++) { foo(); }`);
    gc();
}
