//@ runDefault("--useConcurrentJIT=0")

function foo() {
    let x = 10000000000n
    for (let i = 0; i <= 100000; i++) {
        let a0 = [];
        a0[x++];
        x = 0;
    }
}

foo();
foo();
