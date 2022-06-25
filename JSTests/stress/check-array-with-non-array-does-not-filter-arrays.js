//@ runDefault("--thresholdForFTLOptimizeAfterWarmUp=0", "--useConcurrentJIT=0")

function foo(...a) {
    for (const w of a) {
        for (const q of arguments) {
        }
    }
}

foo(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13);

for (let i = 0; i < 1000; i++) {
    foo();
}

foo(0);
