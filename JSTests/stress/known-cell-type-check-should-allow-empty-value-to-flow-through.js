//@ runDefault("--jitPolicyScale=0", "--useConcurrentJIT=0")

class C extends class {} {
    constructor(beforeSuper) {
        let f = () => {
            for (let j=0; j<10; j++) {
                try {
                    this.x
                } catch (e) {
                }
            }
        };
        if (beforeSuper) {
            f();
            super();
        } else {
            super();
            f();
        }
    }
};
for (let i = 0; i < 10000; i++) {
    new C(false);
    new C(true);
}
