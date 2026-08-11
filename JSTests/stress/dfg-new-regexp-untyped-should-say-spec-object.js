//@ runDefault("--jitPolicyScale=0.01", "--useConcurrentJIT=0")
function foo() {
    "use strict";

    let i = 0;
    do {
        const x = [];

        const t = new RegExp(x, x);

        parseInt(t, t);

        i ++;
    } while (i <= 8);
}

for (let i = 0; i < 16; i++) {
    foo();
}
