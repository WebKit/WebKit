//@ runDefault("--jitPolicyScale=0", "--useConcurrentJIT=0")

function f0() {
}

function bar() {
    f0(...arguments);
}

const a = new Uint8Array(1);

function foo() {
    bar(0, 0);
    a.find(()=>{});
}


for (let i=0; i<3; i++) {
    foo();
}
