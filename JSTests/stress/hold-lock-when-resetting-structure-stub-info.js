//@ runDefault("--jitPolicyScale=0")

function foo() {
    function bar() {
        class C {
            constructor() {
                this.x = 42;
            }
        }
        let c = new C();
        for (let i=0; i<100; i++) {
            c.x;
        }
    };
    for (let i=0; i<1000; i++) {
        bar();
    }
}

for (let i=0; i<25; i++) {
    runString(`${foo};foo();`);
}
