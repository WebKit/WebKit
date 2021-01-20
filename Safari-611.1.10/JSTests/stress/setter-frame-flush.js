//@ runDefault("--jitPolicyScale=0", "--collectContinuously=1")

function setter(s) {
    +s;
    Array.of(arguments);
}

let o = {};
Object.defineProperty(o, 'f', {
    set: setter
});

function foo() {
    bar(o, 'f');
}

function bar(a0, a1) {
    try {
        a0[a1] = null;
    } catch {}
}

for (let i=0; i < 10000; i++) {
    foo();
}
