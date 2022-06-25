//@ runDefault("--jitPolicyScale=0", "--useConcurrentGC=false", "--useConcurrentJIT=false", "--useGenerationalGC=false")
class A extends Object {
    constructor(beforeSuper) {
        let touchThis = () => {
            try {
                this.x = this.x;
            } catch (e) {
            }
            try {
                this.x = +this.x
            } catch (e) {
            }
        };
        if (beforeSuper) {
            touchThis();
            super();
        } else {
            super();
            touchThis();
        }
    }
}

for (var i = 0; i < 10000; i++) {
    new A(false);
    new A(true);
}
