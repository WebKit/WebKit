//@ runDefault("--thresholdForOptimizeAfterWarmUp=0", "--useTypeProfiler=true")

function foo() {
    try {
        throw 42;
    } catch(e) {
        if (e !== 42)
            throw new Error("Bad!")
    }
}

function test(_a) {
    if (_a === 0) {
        foo();
    } 
}

function bar() {
    test(Intl.NumberFormat());
    test(Intl.NumberFormat());
    test(0);
}

for (let i=0; i<200; i++) {
    bar()
}
